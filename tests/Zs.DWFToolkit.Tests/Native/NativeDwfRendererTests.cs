using FluentAssertions;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Native;
using Xunit;

namespace Zs.DWFToolkit.Tests.Native;

public sealed class NativeDwfRendererTests
{
    // The native library is not deployed next to the test assembly, so the renderer
    // must report unavailable and degrade gracefully rather than throwing.
    private readonly NativeDwfRenderer _renderer = new();

    [Fact]
    public void IsAvailable_ReturnsFalse_WhenNativeLibraryAbsent()
    {
        _renderer.IsAvailable.Should().BeFalse();
    }

    [Fact]
    public async Task RenderPageAsync_ReturnsNativeLibraryNotFound_WhenLibraryAbsent()
    {
        var result = await _renderer.RenderPageAsync("x.dwf", 0, "out.png", new DwfRenderOptions());

        result.Success.Should().BeFalse();
        result.ErrorCode.Should().Be(DwfErrorCodes.NativeLibraryNotFound);
    }
}
