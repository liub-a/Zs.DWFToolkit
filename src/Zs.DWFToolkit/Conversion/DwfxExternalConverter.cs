using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Conversion;

/// <summary>
/// Converts DWFx/XPS through external document engines. Plain DWF conversion only succeeds
/// when a real native W2D/DWF renderer returns full page images; embedded preview/raster
/// resources and thumbnails are intentionally not treated as successful conversion output.
/// </summary>
public sealed class DwfxExternalConverter : IDwfConverter
{
    private readonly IDwfDocumentReader _reader;
    private readonly INativeDwfRenderer? _nativeRenderer;

    public DwfxExternalConverter(
        IDwfDocumentReader? reader = null,
        IDwfThumbnailExtractor? thumbnailExtractor = null,
        INativeDwfRenderer? nativeRenderer = null)
    {
        _reader = reader ?? new Zs.DWFToolkit.Services.DwfDocumentReader();
        _ = thumbnailExtractor; // Kept for source compatibility with earlier constructor usage.
        _nativeRenderer = nativeRenderer;
    }

    public async Task<DwfRenderResult> ConvertToPdfAsync(
        string sourcePath,
        string outputPdfPath,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default)
    {
        options ??= new DwfRenderOptions();
        if (!File.Exists(sourcePath)) return FileFailure(sourcePath);

        var info = await _reader.ReadInfoAsync(sourcePath, cancellationToken).ConfigureAwait(false);
        var kind = info.Success ? info.Kind : FileKindDetector.Detect(sourcePath);

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

        if (kind is DwfFileKind.Dwfx or DwfFileKind.Xps)
            return await ConvertXpsLikeToPdfAsync(sourcePath, outputPdfPath, options, cancellationToken).ConfigureAwait(false);

        if (!info.Success && kind != DwfFileKind.Dwf)
            return FromInfoFailure(info, sourcePath);

        if (kind != DwfFileKind.Dwf)
            return Unsupported(sourcePath, outputDirectory: null, "Unsupported file kind.");

        if (!options.CreatePdfFromImagesFallback)
            return Unsupported(sourcePath, outputDirectory: null, "Plain DWF to PDF requires native rendering followed by image-to-PDF assembly.");

        var tempDir = Path.Combine(Path.GetTempPath(), "zs-dwf-toolkit", "pdf", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);
        try
        {
            var imageResult = await ConvertOrdinaryDwfToImagesAsync(sourcePath, tempDir, ForcePdfFriendlyImageOptions(options), info, cancellationToken).ConfigureAwait(false);
            if (!imageResult.Success || imageResult.OutputFiles.Count == 0)
                return Unsupported(sourcePath, outputDirectory: null, imageResult.ErrorMessage);

            var pdf = await CreatePdfFromImagesAsync(imageResult.OutputFiles, outputPdfPath, options, cancellationToken).ConfigureAwait(false);
            pdf.SourcePath = sourcePath;
            if (pdf.Success)
                pdf.ErrorMessage ??= "PDF was assembled from native-rendered page images; this is raster output, not vector CAD output.";
            return pdf;
        }
        finally
        {
            if (!options.KeepTemporaryFiles)
            {
                try { Directory.Delete(tempDir, recursive: true); } catch { }
            }
        }
    }

    public async Task<DwfRenderResult> ConvertToImagesAsync(
        string sourcePath,
        string outputDirectory,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default)
    {
        options ??= new DwfRenderOptions();
        if (!File.Exists(sourcePath)) return FileFailure(sourcePath, outputDirectory: outputDirectory);

        var info = await _reader.ReadInfoAsync(sourcePath, cancellationToken).ConfigureAwait(false);
        var kind = info.Success ? info.Kind : FileKindDetector.Detect(sourcePath);

        SafePath.EnsureDirectory(outputDirectory);
        if (kind is DwfFileKind.Dwfx or DwfFileKind.Xps)
            return await ConvertXpsLikeToImagesAsync(sourcePath, outputDirectory, options, cancellationToken).ConfigureAwait(false);

        if (!info.Success && kind != DwfFileKind.Dwf)
            return FromInfoFailure(info, sourcePath, outputDirectory);

        if (kind == DwfFileKind.Dwf)
            return await ConvertOrdinaryDwfToImagesAsync(sourcePath, outputDirectory, options, info, cancellationToken).ConfigureAwait(false);

        return Unsupported(sourcePath, outputDirectory, "Unsupported file kind.");
    }

    private async Task<DwfRenderResult> ConvertXpsLikeToPdfAsync(string sourcePath, string outputPdfPath, DwfRenderOptions options, CancellationToken cancellationToken)
    {
        var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
        if (mutool != null)
        {
            var run = await ProcessRunner.RunAsync(mutool, new[] { "convert", "-o", outputPdfPath, sourcePath }, options.Timeout, cancellationToken).ConfigureAwait(false);
            return ExternalPdfResult(run, sourcePath, outputPdfPath, "mutool convert");
        }

        var gxps = ProcessRunner.FindExecutable(options.GhostXpsPath, "gxps", "gxpswin64c", "gxpswin32c");
        if (gxps != null)
        {
            var run = await ProcessRunner.RunAsync(gxps, new[] { "-sDEVICE=pdfwrite", "-dNOPAUSE", "-dBATCH", "-o", outputPdfPath, sourcePath }, options.Timeout, cancellationToken).ConfigureAwait(false);
            return ExternalPdfResult(run, sourcePath, outputPdfPath, "gxps pdfwrite");
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = DwfErrorCodes.ExternalToolNotFound,
            ErrorMessage = "DWFx/XPS conversion requires mutool or GhostXPS/gxps on PATH, or configured paths in DwfRenderOptions.",
            SourcePath = sourcePath
        };
    }

    private async Task<DwfRenderResult> ConvertXpsLikeToImagesAsync(string sourcePath, string outputDirectory, DwfRenderOptions options, CancellationToken cancellationToken)
    {
        var imageFormat = NormalizeImageFormat(options.ImageFormat);
        var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
        if (mutool != null)
        {
            var pattern = Path.Combine(outputDirectory, $"page-%03d.{imageFormat}");
            var args = new List<string> { "draw", "-r", options.Dpi.ToString(System.Globalization.CultureInfo.InvariantCulture), "-o", pattern };
            if (options.WidthPx.HasValue) args.AddRange(new[] { "-w", options.WidthPx.Value.ToString(System.Globalization.CultureInfo.InvariantCulture) });
            if (options.HeightPx.HasValue) args.AddRange(new[] { "-h", options.HeightPx.Value.ToString(System.Globalization.CultureInfo.InvariantCulture) });
            args.Add(sourcePath);

            var run = await ProcessRunner.RunAsync(mutool, args, options.Timeout, cancellationToken).ConfigureAwait(false);
            return ExternalImageResult(run, sourcePath, outputDirectory, imageFormat, "mutool draw");
        }

        var gxps = ProcessRunner.FindExecutable(options.GhostXpsPath, "gxps", "gxpswin64c", "gxpswin32c");
        if (gxps != null)
        {
            var device = imageFormat == "jpg" ? "jpeg" : "png16m";
            var pattern = Path.Combine(outputDirectory, $"page-%03d.{imageFormat}");
            var args = new List<string>
            {
                $"-sDEVICE={device}",
                "-dNOPAUSE",
                "-dBATCH",
                $"-r{options.Dpi.ToString(System.Globalization.CultureInfo.InvariantCulture)}",
                "-o",
                pattern,
                sourcePath
            };

            var run = await ProcessRunner.RunAsync(gxps, args, options.Timeout, cancellationToken).ConfigureAwait(false);
            return ExternalImageResult(run, sourcePath, outputDirectory, imageFormat, $"gxps {device}");
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = DwfErrorCodes.ExternalToolNotFound,
            ErrorMessage = "DWFx/XPS image conversion requires mutool or GhostXPS/gxps.",
            SourcePath = sourcePath,
            OutputDirectory = outputDirectory
        };
    }

    private async Task<DwfRenderResult> ConvertOrdinaryDwfToImagesAsync(
        string sourcePath,
        string outputDirectory,
        DwfRenderOptions options,
        DwfDocumentInfo info,
        CancellationToken cancellationToken)
    {
        var imageFormat = NormalizeImageFormat(options.ImageFormat);
        var pageCount = Math.Max(info.PageCount, 1);

        if (options.PreferNativeDwfRenderer && _nativeRenderer is { IsAvailable: true })
        {
            var outputFiles = new List<string>();
            for (var i = 0; i < pageCount; i++)
            {
                var outFile = Path.Combine(outputDirectory, $"page-{i + 1:000}.{imageFormat}");
                var result = await _nativeRenderer.RenderPageAsync(sourcePath, i, outFile, options, cancellationToken).ConfigureAwait(false);
                if (result.Success && File.Exists(outFile)) outputFiles.Add(outFile);
            }

            if (outputFiles.Count > 0)
            {
                return new DwfRenderResult
                {
                    Success = true,
                    ErrorCode = DwfErrorCodes.Ok,
                    SourcePath = sourcePath,
                    OutputDirectory = outputDirectory,
                    OutputFiles = outputFiles,
                    ToolName = "native-dwf-renderer"
                };
            }
        }

        return Unsupported(sourcePath, outputDirectory, "Plain DWF to image requires a real native W2D/DWF renderer. Embedded raster/page-preview resources and thumbnails are intentionally not accepted as conversion output because they are often incomplete.");
    }

    private async Task<DwfRenderResult> CreatePdfFromImagesAsync(IReadOnlyList<string> imageFiles, string outputPdfPath, DwfRenderOptions options, CancellationToken cancellationToken)
    {
        var mutool = ProcessRunner.FindExecutable(options.MutoolPath, "mutool");
        if (mutool != null)
        {
            var args = new List<string> { "convert", "-o", outputPdfPath };
            args.AddRange(imageFiles);
            var run = await ProcessRunner.RunAsync(mutool, args, options.Timeout, cancellationToken).ConfigureAwait(false);
            if (run.Success && File.Exists(outputPdfPath)) return ExternalPdfResult(run, string.Empty, outputPdfPath, "images + mutool convert");
        }

        if (SimpleImagePdfWriter.TryWrite(imageFiles, outputPdfPath, options, out var message))
        {
            return new DwfRenderResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                OutputPath = outputPdfPath,
                OutputFiles = new List<string> { outputPdfPath },
                ToolName = "simple-image-pdf-writer",
                ErrorMessage = message
            };
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = DwfErrorCodes.ExternalToolNotFound,
            ErrorMessage = "Could not assemble PDF from images. Install mutool or use JPEG/RGB PNG images supported by the simple writer.",
            OutputPath = outputPdfPath
        };
    }

    private static DwfRenderOptions ForcePdfFriendlyImageOptions(DwfRenderOptions options) => new()
    {
        Dpi = options.Dpi,
        WidthPx = options.WidthPx,
        HeightPx = options.HeightPx,
        ImageFormat = "jpg",
        JpegQuality = options.JpegQuality,
        Timeout = options.Timeout,
        MutoolPath = options.MutoolPath,
        GhostXpsPath = options.GhostXpsPath,
        PreferNativeDwfRenderer = options.PreferNativeDwfRenderer,
        CreatePdfFromImagesFallback = options.CreatePdfFromImagesFallback,
        PdfPageWidthPoints = options.PdfPageWidthPoints,
        PdfPageHeightPoints = options.PdfPageHeightPoints,
        PdfMarginPoints = options.PdfMarginPoints,
        KeepTemporaryFiles = options.KeepTemporaryFiles,
        Overwrite = true
    };

    private static DwfRenderResult ExternalPdfResult(ProcessRunResult run, string sourcePath, string outputPdfPath, string toolName) => new()
    {
        Success = run.Success && File.Exists(outputPdfPath),
        ErrorCode = run.Success && File.Exists(outputPdfPath) ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
        ErrorMessage = run.Success ? null : $"{toolName} failed.",
        SourcePath = sourcePath,
        OutputPath = outputPdfPath,
        OutputFiles = File.Exists(outputPdfPath) ? new List<string> { outputPdfPath } : new List<string>(),
        ToolName = toolName,
        StdOut = run.StdOut,
        StdErr = run.StdErr
    };

    private static DwfRenderResult ExternalImageResult(ProcessRunResult run, string sourcePath, string outputDirectory, string imageFormat, string toolName)
    {
        var files = FindOutputImages(outputDirectory, imageFormat);
        return new DwfRenderResult
        {
            Success = run.Success && files.Count > 0,
            ErrorCode = run.Success && files.Count > 0 ? DwfErrorCodes.Ok : run.ErrorCode ?? DwfErrorCodes.ExternalToolFailed,
            ErrorMessage = run.Success ? null : $"{toolName} failed.",
            SourcePath = sourcePath,
            OutputDirectory = outputDirectory,
            OutputFiles = files,
            ToolName = toolName,
            StdOut = run.StdOut,
            StdErr = run.StdErr
        };
    }

    private static DwfRenderResult Unsupported(string sourcePath, string? outputDirectory, string? message) => new()
    {
        Success = false,
        ErrorCode = DwfErrorCodes.UnsupportedDwfRendering,
        ErrorMessage = message ?? "Unsupported DWF rendering.",
        SourcePath = sourcePath,
        OutputDirectory = outputDirectory
    };

    private static DwfRenderResult FileFailure(string sourcePath, string? outputDirectory = null) => new()
    {
        Success = false,
        ErrorCode = DwfErrorCodes.FileNotFound,
        ErrorMessage = "Source file not found.",
        SourcePath = sourcePath,
        OutputDirectory = outputDirectory
    };

    private static DwfRenderResult FromInfoFailure(DwfDocumentInfo info, string sourcePath, string? outputDirectory = null) => new()
    {
        Success = false,
        ErrorCode = info.ErrorCode ?? DwfErrorCodes.ReadFailed,
        ErrorMessage = info.ErrorMessage,
        SourcePath = sourcePath,
        OutputDirectory = outputDirectory
    };

    private static string NormalizeImageFormat(string? format)
    {
        var f = string.IsNullOrWhiteSpace(format) ? "png" : format.Trim().TrimStart('.').ToLowerInvariant();
        return f is "jpeg" ? "jpg" : f;
    }

    private static List<string> FindOutputImages(string outputDirectory, string imageFormat)
    {
        if (!Directory.Exists(outputDirectory)) return new List<string>();
        return Directory.EnumerateFiles(outputDirectory, $"*.{imageFormat}", SearchOption.TopDirectoryOnly)
            .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }
}
