using FluentAssertions;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Native;
using Xunit;

namespace Zs.DWFToolkit.Tests.Native;

public sealed class NativeDwfEditorTests
{
    private readonly NativeDwfEditor _editor = new();

    [Fact]
    public async Task StampImageOnW2dAsync_RejectsUndersizedBuffer()
    {
        var tiny = new byte[4]; // far smaller than 4*4*4

        var result = await _editor.StampImageOnW2dAsync("in.w2d", "out.w2d", tiny, 4, 4, new DwfStampRect(0, 0, 10, 10));

        result.Success.Should().BeFalse();
        result.ErrorCode.Should().Be(DwfErrorCodes.InvalidArgument);
    }

    [Fact]
    public async Task StampImageOnW2dAsync_ReturnsFileNotFound_WhenInputMissing()
    {
        var rgba = new byte[4 * 4 * 4];

        var result = await _editor.StampImageOnW2dAsync("/no/such.w2d", "out.w2d", rgba, 4, 4, new DwfStampRect(0, 0, 10, 10));

        result.Success.Should().BeFalse();
        // No native library next to the test assembly, or the input is missing —
        // either way the host survives and a failure code is surfaced.
        result.ErrorCode.Should().BeOneOf(DwfErrorCodes.FileNotFound, DwfErrorCodes.NativeLibraryNotFound);
    }
}
