using System.Runtime.InteropServices;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Native;

/// <summary>
/// In-process DWF/W2D editor backed by the native library (P/Invoke). Currently
/// supports stamping an image onto a W2D stream; DWF-package repackaging is future.
/// </summary>
public sealed class NativeDwfEditor : IDwfEditor
{
    public bool IsAvailable
    {
        get
        {
            try { NativeMethods.ProbeLoad(); return true; }
            catch (DllNotFoundException) { return false; }
            catch (EntryPointNotFoundException) { return false; }
            catch (BadImageFormatException) { return false; }
        }
    }

    public Task<DwfRenderResult> StampImageOnW2dAsync(
        string inputW2dPath,
        string outputW2dPath,
        byte[] rgbaPixels,
        int imageWidth,
        int imageHeight,
        DwfStampRect rect,
        CancellationToken cancellationToken = default)
    {
        if (rgbaPixels is null || imageWidth <= 0 || imageHeight <= 0 ||
            rgbaPixels.Length < (long)imageWidth * imageHeight * 4)
        {
            return Task.FromResult(Fail(DwfErrorCodes.InvalidArgument,
                "rgbaPixels must be imageWidth*imageHeight*4 bytes.", inputW2dPath, outputW2dPath));
        }
        if (!File.Exists(inputW2dPath))
            return Task.FromResult(Fail(DwfErrorCodes.FileNotFound, "Input W2D not found.", inputW2dPath, outputW2dPath));

        try
        {
            Internal.SafePath.EnsureParentDirectory(outputW2dPath);
            var code = NativeMethods.StampW2dImage(
                inputW2dPath, outputW2dPath, rgbaPixels, rgbaPixels.Length,
                imageWidth, imageHeight, rect.MinX, rect.MinY, rect.MaxX, rect.MaxY);

            if (code != 0)
            {
                return Task.FromResult(Fail(DwfErrorCodes.NativeRenderFailed,
                    NativeMethods.GetLastErrorString() ?? $"Stamp failed with code {code}.",
                    inputW2dPath, outputW2dPath));
            }

            return Task.FromResult(new DwfRenderResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = inputW2dPath,
                OutputPath = outputW2dPath,
                OutputFiles = { outputW2dPath },
                ToolName = "native-dwf-editor"
            });
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            return Task.FromResult(Fail(DwfErrorCodes.NativeLibraryNotFound, ex.Message, inputW2dPath, outputW2dPath));
        }
    }

    private static DwfRenderResult Fail(string code, string message, string src, string outPath) => new()
    {
        Success = false,
        ErrorCode = code,
        ErrorMessage = message,
        SourcePath = src,
        OutputPath = outPath,
        ToolName = "native-dwf-editor"
    };
}
