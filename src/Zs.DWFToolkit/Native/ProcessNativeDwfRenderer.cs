using System.Text.Json;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Native;

/// <summary>
/// Renders DWF/W2D out-of-process by invoking the native <c>zs_dwf_worker</c>
/// executable. A crash or hang in the native renderer is isolated from the host:
/// the worker is run with a timeout and killed if it exceeds it, and a non-zero
/// exit surfaces as a render failure instead of taking down the process.
/// </summary>
public sealed class ProcessNativeDwfRenderer : INativeDwfRenderer
{
    private readonly string? _workerPath;

    public ProcessNativeDwfRenderer(string? workerPath = null)
    {
        _workerPath = ResolveWorker(workerPath);
    }

    public bool IsAvailable => _workerPath != null;

    public async Task<DwfDocumentInfo> GetInfoAsync(string sourcePath, CancellationToken cancellationToken = default)
    {
        var run = await RunAsync(new[] { "info", sourcePath }, TimeSpan.FromSeconds(60), cancellationToken).ConfigureAwait(false);
        if (run is null)
            return Unavailable(sourcePath);

        var info = TryDeserialize<DwfDocumentInfo>(run.StdOut);
        if (info != null) return info;

        return new DwfDocumentInfo
        {
            Success = false,
            ErrorCode = MapFailure(run),
            ErrorMessage = FailureMessage(run, "info"),
            SourcePath = sourcePath,
            Kind = DwfFileKind.Dwf
        };
    }

    public Task<DwfRenderResult> RenderPageAsync(string sourcePath, int pageIndex, string outputPath, DwfRenderOptions options, CancellationToken cancellationToken = default)
        => RenderAsync(
            new[] { "page", sourcePath, pageIndex.ToString(System.Globalization.CultureInfo.InvariantCulture), outputPath,
                    Width(options), Height(options), options.Dpi.ToString(System.Globalization.CultureInfo.InvariantCulture) },
            sourcePath, pageIndex, outputPath, options, cancellationToken);

    public Task<DwfRenderResult> RenderW2dFileAsync(string sourcePath, string outputPath, DwfRenderOptions options, CancellationToken cancellationToken = default)
        => RenderAsync(
            new[] { "w2d", sourcePath, outputPath, Width(options), Height(options), options.Dpi.ToString(System.Globalization.CultureInfo.InvariantCulture) },
            sourcePath, 0, outputPath, options, cancellationToken);

    private async Task<DwfRenderResult> RenderAsync(string[] args, string sourcePath, int pageIndex, string outputPath, DwfRenderOptions options, CancellationToken cancellationToken)
    {
        var run = await RunAsync(args, options.Timeout, cancellationToken).ConfigureAwait(false);
        if (run is null)
        {
            return new DwfRenderResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NativeLibraryNotFound,
                ErrorMessage = "Native render worker (zs_dwf_worker) was not found.",
                SourcePath = sourcePath,
                PageIndex = pageIndex,
                OutputPath = outputPath,
                ToolName = "native-dwf-worker"
            };
        }

        var result = TryDeserialize<DwfRenderResult>(run.StdOut);
        if (result != null)
        {
            result.SourcePath = sourcePath;
            result.ToolName ??= "native-dwf-worker";
            return result;
        }

        return new DwfRenderResult
        {
            Success = false,
            ErrorCode = MapFailure(run),
            ErrorMessage = FailureMessage(run, "render"),
            SourcePath = sourcePath,
            PageIndex = pageIndex,
            OutputPath = outputPath,
            ToolName = "native-dwf-worker"
        };
    }

    private async Task<ProcessRunResult?> RunAsync(string[] args, TimeSpan timeout, CancellationToken cancellationToken)
    {
        if (_workerPath is null) return null;
        return await ProcessRunner.RunAsync(_workerPath, args, timeout, cancellationToken).ConfigureAwait(false);
    }

    // A timeout (worker killed) maps to Timeout; any other non-zero exit is a
    // native render failure that did not bring the host down.
    private static string MapFailure(ProcessRunResult run)
        => run.ErrorCode == DwfErrorCodes.Timeout ? DwfErrorCodes.Timeout : DwfErrorCodes.NativeRenderFailed;

    private static string FailureMessage(ProcessRunResult run, string op)
        => run.ErrorCode == DwfErrorCodes.Timeout
            ? $"Native worker timed out during {op} and was terminated."
            : $"Native worker {op} failed (exit {run.ExitCode}). {Trim(run.StdErr)}";

    private static string Trim(string s) => s.Length > 400 ? s[..400] : s;

    private static string Width(DwfRenderOptions o) => (o.WidthPx ?? 2400).ToString(System.Globalization.CultureInfo.InvariantCulture);
    private static string Height(DwfRenderOptions o) => (o.HeightPx ?? 1600).ToString(System.Globalization.CultureInfo.InvariantCulture);

    private static T? TryDeserialize<T>(string stdout) where T : class
    {
        var json = ExtractJson(stdout);
        if (json is null) return null;
        try { return JsonSerializer.Deserialize<T>(json, new JsonSerializerOptions { PropertyNameCaseInsensitive = true }); }
        catch { return null; }
    }

    private static string? ExtractJson(string stdout)
    {
        var start = stdout.IndexOf('{');
        var end = stdout.LastIndexOf('}');
        return start >= 0 && end > start ? stdout[start..(end + 1)] : null;
    }

    private DwfDocumentInfo Unavailable(string sourcePath) => new()
    {
        Success = false,
        ErrorCode = DwfErrorCodes.NativeLibraryNotFound,
        ErrorMessage = "Native render worker (zs_dwf_worker) was not found.",
        SourcePath = sourcePath,
        Kind = DwfFileKind.Dwf
    };

    private static string? ResolveWorker(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath) && File.Exists(explicitPath))
            return explicitPath;

        var env = Environment.GetEnvironmentVariable("ZS_DWF_WORKER");
        if (!string.IsNullOrWhiteSpace(env) && File.Exists(env))
            return env;

        var exe = OperatingSystem.IsWindows() ? "zs_dwf_worker.exe" : "zs_dwf_worker";
        var baseDir = AppContext.BaseDirectory;
        var candidate = Path.Combine(baseDir, exe);
        return File.Exists(candidate) ? candidate : null;
    }
}
