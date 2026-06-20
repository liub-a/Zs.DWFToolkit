using System.Runtime.InteropServices;
using FluentAssertions;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Native;
using Xunit;

namespace Zs.DWFToolkit.Tests.Native;

public sealed class ProcessNativeDwfRendererTests
{
    [Fact]
    public void IsAvailable_False_WhenWorkerMissing()
    {
        var renderer = new ProcessNativeDwfRenderer("/no/such/zs_dwf_worker");

        renderer.IsAvailable.Should().BeFalse();
    }

    [Fact]
    public async Task RenderPageAsync_ReturnsNativeLibraryNotFound_WhenWorkerMissing()
    {
        var renderer = new ProcessNativeDwfRenderer("/no/such/zs_dwf_worker");

        var result = await renderer.RenderPageAsync("x.dwf", 0, "out.png", new DwfRenderOptions());

        result.Success.Should().BeFalse();
        result.ErrorCode.Should().Be(DwfErrorCodes.NativeLibraryNotFound);
    }

    [Fact]
    public async Task RenderPageAsync_IsolatesWorkerCrash_AsRenderFailure()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) return; // shell-script fake worker is POSIX

        var worker = WriteFakeWorker("#!/bin/sh\necho 'boom' 1>&2\nexit 1\n");
        try
        {
            var renderer = new ProcessNativeDwfRenderer(worker);
            renderer.IsAvailable.Should().BeTrue();

            var result = await renderer.RenderPageAsync("x.dwf", 0, "out.png", new DwfRenderOptions());

            // The host survived a crashing worker; the failure is surfaced, not thrown.
            result.Success.Should().BeFalse();
            result.ErrorCode.Should().Be(DwfErrorCodes.NativeRenderFailed);
        }
        finally { File.Delete(worker); }
    }

    [Fact]
    public async Task RenderPageAsync_IsolatesWorkerHang_AsTimeout()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) return;

        var worker = WriteFakeWorker("#!/bin/sh\nsleep 10\n");
        try
        {
            var renderer = new ProcessNativeDwfRenderer(worker);
            var options = new DwfRenderOptions { Timeout = TimeSpan.FromMilliseconds(300) };

            var result = await renderer.RenderPageAsync("x.dwf", 0, "out.png", options);

            result.Success.Should().BeFalse();
            result.ErrorCode.Should().Be(DwfErrorCodes.Timeout);
        }
        finally { File.Delete(worker); }
    }

    private static string WriteFakeWorker(string script)
    {
        var path = Path.Combine(Path.GetTempPath(), "zs-fake-worker-" + Guid.NewGuid().ToString("N") + ".sh");
        File.WriteAllText(path, script);
#pragma warning disable CA1416
        File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute);
#pragma warning restore CA1416
        return path;
    }
}
