using FluentAssertions;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Services;
using Xunit;

namespace Zs.DWFToolkit.Tests.Services;

public sealed class DwfDocumentReaderTests
{
    private readonly DwfDocumentReader _reader = new();

    [Fact]
    public async Task ReadInfoAsync_Fails_WhenFileMissing()
    {
        var result = await _reader.ReadInfoAsync("/no/such/file.dwf");

        result.Success.Should().BeFalse();
        result.ErrorCode.Should().Be(DwfErrorCodes.FileNotFound);
    }

    [Fact]
    public async Task ReadInfoAsync_Fails_WhenNotZipBased()
    {
        var path = Path.GetTempFileName();
        File.WriteAllText(path, "plain text not a package");
        try
        {
            var result = await _reader.ReadInfoAsync(path);

            result.Success.Should().BeFalse();
            result.ErrorCode.Should().Be(DwfErrorCodes.UnsupportedFormat);
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public async Task ReadInfoAsync_DiscoversXpsFixedPages_ForDwfx()
    {
        var path = TestData.WritePackage(".dwfx", new Dictionary<string, byte[]>
        {
            ["[Content_Types].xml"] = TestData.Utf8("<Types/>"),
            ["Documents/1/Pages/1.fpage"] = TestData.Utf8("<FixedPage Width=\"1190\" Height=\"840\"/>")
        });
        try
        {
            var result = await _reader.ReadInfoAsync(path);

            result.Success.Should().BeTrue();
            result.Kind.Should().Be(DwfFileKind.Dwfx);
            result.Pages.Should().ContainSingle();
            result.Pages[0].IsXpsFixedPage.Should().BeTrue();
            result.Pages[0].Width.Should().Be(1190);
            result.Pages[0].Height.Should().Be(840);
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public async Task ReadInfoAsync_MarksPagesAs3d_ForW3dPackage()
    {
        var path = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["model.w3d"] = new byte[] { 1, 2, 3 }
        });
        try
        {
            var result = await _reader.ReadInfoAsync(path);

            result.Success.Should().BeTrue();
            result.Pages.Should().ContainSingle();
            result.Pages[0].Has3dGraphics.Should().BeTrue();
            result.Pages[0].Has2dGraphics.Should().BeFalse();
        }
        finally { File.Delete(path); }
    }
}
