using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Conversion;

/// <summary>
/// Uses external document engines to convert DWFx/XPS.
/// Recommended tools:
///   - mutool from MuPDF: mutool convert/draw
///   - GhostXPS/gxps if available
/// This class does not ship those tools.
/// </summary>
public sealed class DwfxExternalConverter : IDwfConverter
{
    private readonly IDwfDocumentReader _reader;
    private readonly IDwfThumbnailExtractor _thumbnailExtractor;
    private readonly INativeDwfRenderer? _nativeRenderer;

    public DwfxExternalConverter(
        IDwfDocumentReader? reader = null,
        IDwfThumbnailExtractor? thumbnailExtractor = null,
        INativeDwfRenderer? nativeRenderer = null)
    {
        _reader = reader ?? new Zs.DWFToolkit.Services.DwfDocumentReader();
        _thumbnailExtractor = thumbnailExtractor ?? new DwfThumbnailExtractor();
        _nativeRenderer = nativeRenderer;
    }

    public async Task<DwfRenderResult> ConvertToPdfAsync(
        string sourcePath,
        string outputPdfPath,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default)
    {
        options ??= new DwfRenderOptions();
        var info = await _reader.ReadInfoAsync(sourcePath, cancellationToken).ConfigureAwait(false);
        if (!info.Success)
            return FromInfoFailure(info, sourcePath);

        SafePath.EnsureParentDirectory(outputPdfPath);
        if (File.Exists(outputPdfPath) && !options.Overwrite)
        {
            return new DwfRenderResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = sourcePath,
                OutputPath = outputPdfPath,
                OutputFiles = { outputPdfPath },
                ToolName = "existing"
            };
        }

        if (info.Kind is DwfFileKind.Dwfx or DwfFileKind.Xps)
        {
            var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
            if (mutool != null)
            {
                var run = await ProcessRunner.RunAsync(
                    mutool,
                    new[] { "convert", "-o", outputPdfPath, sourcePath },
                    options.Timeout,
                    cancellationToken).ConfigureAwait(false);

                return new DwfRenderResult
                {
                    Success = run.Success && File.Exists(outputPdfPath),
                    ErrorCode = run.Success && File.Exists(outputPdfPath) ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
                    ErrorMessage = run.Success ? null : "mutool convert failed.",
                    SourcePath = sourcePath,
                    OutputPath = outputPdfPath,
                    OutputFiles = File.Exists(outputPdfPath) ? new List<string> { outputPdfPath } : new List<string>(),
                    ToolName = "mutool convert",
                    StdOut = run.StdOut,
                    StdErr = run.StdErr
                };
            }

            var gxps = ProcessRunner.FindExecutable(options.GhostXpsPath, "gxps", "gxpswin64c", "gxpswin32c");
            if (gxps != null)
            {
                var run = await ProcessRunner.RunAsync(
                    gxps,
                    new[] { "-sDEVICE=pdfwrite", "-dNOPAUSE", "-dBATCH", "-o", outputPdfPath, sourcePath },
                    options.Timeout,
                    cancellationToken).ConfigureAwait(false);

                return new DwfRenderResult
                {
                    Success = run.Success && File.Exists(outputPdfPath),
                    ErrorCode = run.Success && File.Exists(outputPdfPath) ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
                    ErrorMessage = run.Success ? null : "GhostXPS/gxps PDF conversion failed.",
                    SourcePath = sourcePath,
                    OutputPath = outputPdfPath,
                    OutputFiles = File.Exists(outputPdfPath) ? new List<string> { outputPdfPath } : new List<string>(),
                    ToolName = "gxps pdfwrite",
                    StdOut = run.StdOut,
                    StdErr = run.StdErr
                };
            }

            return new DwfRenderResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.ExternalToolNotFound,
                ErrorMessage = "DWFx/XPS conversion requires mutool or GhostXPS/gxps on PATH, or configured paths in DwfRenderOptions.",
                SourcePath = sourcePath
            };
        }

        // Ordinary DWF PDF conversion requires a real DWF renderer. As a best-effort fallback,
        // extract an embedded thumbnail/preview and wrap it into a PDF using mutool if available.
        // This is useful for list preview, but it is not a drawing-grade PDF.
        if (options.ExtractThumbnailFallback)
        {
            var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
            if (mutool != null)
            {
                var tempDir = Path.Combine(Path.GetTempPath(), "zs-dwf-toolkit", Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(tempDir);
                var thumbPath = Path.Combine(tempDir, "thumbnail.png");

                try
                {
                    var thumb = await _thumbnailExtractor.ExtractBestThumbnailAsync(sourcePath, thumbPath, cancellationToken).ConfigureAwait(false);
                    if (thumb.Success && File.Exists(thumbPath))
                    {
                        var run = await ProcessRunner.RunAsync(
                            mutool,
                            new[] { "convert", "-o", outputPdfPath, thumbPath },
                            options.Timeout,
                            cancellationToken).ConfigureAwait(false);

                        return new DwfRenderResult
                        {
                            Success = run.Success && File.Exists(outputPdfPath),
                            ErrorCode = run.Success && File.Exists(outputPdfPath) ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
                            ErrorMessage = run.Success ? "Plain DWF renderer is not implemented; PDF was created from embedded thumbnail fallback." : "Thumbnail-to-PDF fallback failed.",
                            SourcePath = sourcePath,
                            OutputPath = outputPdfPath,
                            OutputFiles = File.Exists(outputPdfPath) ? new List<string> { outputPdfPath } : new List<string>(),
                            ToolName = "thumbnail-fallback + mutool convert",
                            StdOut = run.StdOut,
                            StdErr = run.StdErr
                        };
                    }
                }
                finally
                {
                    try { Directory.Delete(tempDir, recursive: true); } catch { }
                }
            }
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = DwfErrorCodes.UnsupportedDwfRendering,
            ErrorMessage = "Plain DWF to PDF requires a native W2D/DWF renderer. Current fallback can only create a PDF from embedded thumbnail when mutool is available.",
            SourcePath = sourcePath
        };
    }

    public async Task<DwfRenderResult> ConvertToImagesAsync(
        string sourcePath,
        string outputDirectory,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default)
    {
        options ??= new DwfRenderOptions();
        var info = await _reader.ReadInfoAsync(sourcePath, cancellationToken).ConfigureAwait(false);
        if (!info.Success)
            return FromInfoFailure(info, sourcePath);

        SafePath.EnsureDirectory(outputDirectory);
        var imageFormat = NormalizeImageFormat(options.ImageFormat);

        if (info.Kind is DwfFileKind.Dwfx or DwfFileKind.Xps)
        {
            var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
            if (mutool != null)
            {
                var pattern = Path.Combine(outputDirectory, $"page-%03d.{imageFormat}");
                var args = new List<string> { "draw", "-r", options.Dpi.ToString(System.Globalization.CultureInfo.InvariantCulture), "-o", pattern };

                if (options.WidthPx.HasValue)
                    args.AddRange(new[] { "-w", options.WidthPx.Value.ToString(System.Globalization.CultureInfo.InvariantCulture) });
                if (options.HeightPx.HasValue)
                    args.AddRange(new[] { "-h", options.HeightPx.Value.ToString(System.Globalization.CultureInfo.InvariantCulture) });

                args.Add(sourcePath);

                var run = await ProcessRunner.RunAsync(mutool, args, options.Timeout, cancellationToken).ConfigureAwait(false);
                var files = Directory.Exists(outputDirectory)
                    ? Directory.GetFiles(outputDirectory, $"page-*.{imageFormat}").OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList()
                    : new List<string>();

                return new DwfRenderResult
                {
                    Success = run.Success && files.Count > 0,
                    ErrorCode = run.Success && files.Count > 0 ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
                    ErrorMessage = run.Success ? null : "mutool draw failed.",
                    SourcePath = sourcePath,
                    OutputDirectory = outputDirectory,
                    OutputFiles = files,
                    ToolName = "mutool draw",
                    StdOut = run.StdOut,
                    StdErr = run.StdErr
                };
            }

            return new DwfRenderResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.ExternalToolNotFound,
                ErrorMessage = "DWFx/XPS image conversion requires mutool on PATH or DwfRenderOptions.MutoolPath.",
                SourcePath = sourcePath,
                OutputDirectory = outputDirectory
            };
        }

        if (options.PreferNativeDwfRenderer && _nativeRenderer is { IsAvailable: true })
        {
            var outputFiles = new List<string>();
            for (var i = 0; i < Math.Max(info.PageCount, 1); i++)
            {
                var outFile = Path.Combine(outputDirectory, $"page-{i + 1:000}.{imageFormat}");
                var result = await _nativeRenderer.RenderPageAsync(sourcePath, i, outFile, options, cancellationToken).ConfigureAwait(false);
                if (result.Success && File.Exists(outFile)) outputFiles.Add(outFile);
            }

            return new DwfRenderResult
            {
                Success = outputFiles.Count > 0,
                ErrorCode = outputFiles.Count > 0 ? DwfErrorCodes.Ok : DwfErrorCodes.NativeRenderFailed,
                ErrorMessage = outputFiles.Count > 0 ? null : "Native DWF renderer did not produce any image.",
                SourcePath = sourcePath,
                OutputDirectory = outputDirectory,
                OutputFiles = outputFiles,
                ToolName = "native-dwf-renderer"
            };
        }

        if (options.ExtractThumbnailFallback)
        {
            var thumbPath = Path.Combine(outputDirectory, $"thumbnail.{imageFormat}");
            var thumb = await _thumbnailExtractor.ExtractBestThumbnailAsync(sourcePath, thumbPath, cancellationToken).ConfigureAwait(false);
            if (thumb.Success)
            {
                return new DwfRenderResult
                {
                    Success = true,
                    ErrorCode = DwfErrorCodes.Ok,
                    ErrorMessage = "Plain DWF rendering is not implemented; extracted embedded thumbnail as fallback.",
                    SourcePath = sourcePath,
                    OutputDirectory = outputDirectory,
                    OutputPath = thumb.OutputPath,
                    OutputFiles = new List<string> { thumb.OutputPath! },
                    ToolName = "thumbnail-fallback"
                };
            }
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = DwfErrorCodes.UnsupportedDwfRendering,
            ErrorMessage = "Plain DWF to image requires a native W2D/DWF renderer implementation. This package includes the bridge and fallback extraction, not a complete W2D renderer.",
            SourcePath = sourcePath,
            OutputDirectory = outputDirectory
        };
    }

    private static DwfRenderResult FromInfoFailure(DwfDocumentInfo info, string sourcePath) => new()
    {
        Success = false,
        ErrorCode = info.ErrorCode,
        ErrorMessage = info.ErrorMessage,
        SourcePath = sourcePath
    };

    private static string NormalizeImageFormat(string? format)
    {
        var f = (format ?? "png").Trim().TrimStart('.').ToLowerInvariant();
        return f switch
        {
            "jpg" => "jpg",
            "jpeg" => "jpg",
            "png" => "png",
            _ => "png"
        };
    }
}
