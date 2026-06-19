using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Native;

public sealed class NativeDwfRenderer : INativeDwfRenderer
{
    public bool IsAvailable
    {
        get
        {
            try
            {
                _ = NativeMethods.GetLastErrorString();
                return true;
            }
            catch (DllNotFoundException)
            {
                return false;
            }
            catch (EntryPointNotFoundException)
            {
                return false;
            }
            catch (BadImageFormatException)
            {
                return false;
            }
        }
    }

    public Task<DwfDocumentInfo> GetInfoAsync(string sourcePath, CancellationToken cancellationToken = default)
    {
        var buffer = new StringBuilder(1024 * 1024);
        try
        {
            var code = NativeMethods.GetInfoJson(sourcePath, buffer, buffer.Capacity);
            if (code != 0)
            {
                return Task.FromResult(new DwfDocumentInfo
                {
                    Success = false,
                    ErrorCode = DwfErrorCodes.NativeRenderFailed,
                    ErrorMessage = NativeMethods.GetLastErrorString() ?? $"Native renderer failed with code {code}.",
                    SourcePath = sourcePath,
                    Kind = DwfFileKind.Dwf
                });
            }

            var info = JsonSerializer.Deserialize<DwfDocumentInfo>(buffer.ToString(), JsonOptions()) ?? new DwfDocumentInfo
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NativeRenderFailed,
                ErrorMessage = "Native renderer returned empty info.",
                SourcePath = sourcePath
            };
            return Task.FromResult(info);
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            return Task.FromResult(new DwfDocumentInfo
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NativeLibraryNotFound,
                ErrorMessage = ex.Message,
                SourcePath = sourcePath,
                Kind = DwfFileKind.Dwf
            });
        }
    }

    public Task<DwfRenderResult> RenderPageAsync(
        string sourcePath,
        int pageIndex,
        string outputPath,
        DwfRenderOptions options,
        CancellationToken cancellationToken = default)
    {
        var buffer = new StringBuilder(1024 * 1024);
        try
        {
            var code = NativeMethods.RenderPage(
                sourcePath,
                pageIndex,
                outputPath,
                options.WidthPx ?? 2400,
                options.HeightPx ?? 1600,
                options.Dpi,
                buffer,
                buffer.Capacity);

            if (code != 0)
            {
                return Task.FromResult(new DwfRenderResult
                {
                    Success = false,
                    ErrorCode = DwfErrorCodes.NativeRenderFailed,
                    ErrorMessage = NativeMethods.GetLastErrorString() ?? $"Native renderer failed with code {code}.",
                    SourcePath = sourcePath,
                    PageIndex = pageIndex,
                    OutputPath = outputPath,
                    ToolName = "native-dwf-renderer"
                });
            }

            var result = JsonSerializer.Deserialize<DwfRenderResult>(buffer.ToString(), JsonOptions()) ?? new DwfRenderResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NativeRenderFailed,
                ErrorMessage = "Native renderer returned empty result.",
                SourcePath = sourcePath,
                PageIndex = pageIndex,
                OutputPath = outputPath
            };
            return Task.FromResult(result);
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            return Task.FromResult(new DwfRenderResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NativeLibraryNotFound,
                ErrorMessage = ex.Message,
                SourcePath = sourcePath,
                PageIndex = pageIndex,
                OutputPath = outputPath,
                ToolName = "native-dwf-renderer"
            });
        }
    }

    private static JsonSerializerOptions JsonOptions() => new() { PropertyNameCaseInsensitive = true };
}
