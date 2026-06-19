using FluentAssertions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;
using Xunit;

namespace Zs.DWFToolkit.Tests.Internal;

public sealed class FileKindDetectorTests
{
    [Theory]
    [InlineData("a.dwf", DwfFileKind.Dwf)]
    [InlineData("a.dwfx", DwfFileKind.Dwfx)]
    [InlineData("a.xps", DwfFileKind.Xps)]
    [InlineData("a.bin", DwfFileKind.Unknown)]
    public void Detect_UsesExtension(string name, DwfFileKind expected)
    {
        FileKindDetector.Detect(name).Should().Be(expected);
    }

    [Fact]
    public void IsZipBased_ReturnsTrue_ForPkSignature()
    {
        var path = TestData.WritePackage(".dwfx", new Dictionary<string, byte[]>
        {
            ["x.txt"] = TestData.Utf8("hello")
        });
        try
        {
            FileKindDetector.IsZipBased(path).Should().BeTrue();
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void IsZipBased_ReturnsFalse_ForPlainFile()
    {
        var path = Path.GetTempFileName();
        File.WriteAllText(path, "not a zip");
        try
        {
            FileKindDetector.IsZipBased(path).Should().BeFalse();
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void DetectFromZipEntries_ClassifiesW2dPackageAsDwf()
    {
        var path = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["com.autodesk.dwf.ePlot/sheet.w2d"] = new byte[] { 1, 2, 3 }
        });
        try
        {
            FileKindDetector.DetectFromZipEntries(path, DwfFileKind.Dwf).Should().Be(DwfFileKind.Dwf);
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void DetectFromZipEntries_ClassifiesFixedPagePackageAsDwfx()
    {
        var path = TestData.WritePackage(".dwfx", new Dictionary<string, byte[]>
        {
            ["[Content_Types].xml"] = TestData.Utf8("<Types/>"),
            ["Documents/1/Pages/1.fpage"] = TestData.Utf8("<FixedPage/>")
        });
        try
        {
            FileKindDetector.DetectFromZipEntries(path, DwfFileKind.Dwfx).Should().Be(DwfFileKind.Dwfx);
        }
        finally { File.Delete(path); }
    }
}
