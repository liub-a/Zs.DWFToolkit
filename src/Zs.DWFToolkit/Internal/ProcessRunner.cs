using System.Diagnostics;
using System.Text;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Internal;

internal sealed record ProcessRunResult(
    bool Success,
    int ExitCode,
    string StdOut,
    string StdErr,
    string? ErrorCode,
    string ToolPath);

internal static class ProcessRunner
{
    public static string? FindExecutable(string? explicitPath, params string[] names)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath) && File.Exists(explicitPath))
            return explicitPath;

        var pathEnv = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        var paths = pathEnv.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries);

        foreach (var name in names)
        {
            foreach (var dir in paths)
            {
                var candidate = Path.Combine(dir, name);
                if (File.Exists(candidate)) return candidate;

                if (OperatingSystem.IsWindows())
                {
                    var exe = candidate.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? candidate : candidate + ".exe";
                    if (File.Exists(exe)) return exe;
                }
            }
        }

        return null;
    }

    public static async Task<ProcessRunResult> RunAsync(
        string toolPath,
        IEnumerable<string> args,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        var psi = new ProcessStartInfo
        {
            FileName = toolPath,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (var arg in args)
            psi.ArgumentList.Add(arg);

        using var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        var stdout = new StringBuilder();
        var stderr = new StringBuilder();

        process.OutputDataReceived += (_, e) => { if (e.Data != null) stdout.AppendLine(e.Data); };
        process.ErrorDataReceived += (_, e) => { if (e.Data != null) stderr.AppendLine(e.Data); };

        try
        {
            if (!process.Start())
            {
                return new ProcessRunResult(false, -1, "", "Failed to start process.", DwfErrorCodes.ExternalToolFailed, toolPath);
            }

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeoutCts.CancelAfter(timeout);

            try
            {
                await process.WaitForExitAsync(timeoutCts.Token).ConfigureAwait(false);
            }
            catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
            {
                try { process.Kill(entireProcessTree: true); } catch { }
                return new ProcessRunResult(false, -1, stdout.ToString(), stderr.ToString(), DwfErrorCodes.Timeout, toolPath);
            }

            return new ProcessRunResult(
                process.ExitCode == 0,
                process.ExitCode,
                stdout.ToString(),
                stderr.ToString(),
                process.ExitCode == 0 ? null : DwfErrorCodes.ExternalToolFailed,
                toolPath);
        }
        catch (Exception ex)
        {
            return new ProcessRunResult(false, -1, stdout.ToString(), ex.ToString(), DwfErrorCodes.ExternalToolFailed, toolPath);
        }
    }
}
