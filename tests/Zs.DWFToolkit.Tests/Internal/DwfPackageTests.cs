using FluentAssertions;
using Zs.DWFToolkit.Internal;
using Xunit;

namespace Zs.DWFToolkit.Tests.Internal;

public sealed class DwfPackageTests
{
    [Fact]
    public void FindZipOffset_ReturnsZero_WhenFileStartsWithPk()
    {
        var path = TestData.WritePackage(".dwfx", new Dictionary<string, byte[]> { ["a.txt"] = TestData.Utf8("x") });
        try
        {
            DwfPackage.FindZipOffset(path).Should().Be(0);
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void FindZipOffset_LocatesZip_AfterClassicDwfVersionHeader()
    {
        // Classic DWF v6 layout: "(DWF V06.20)" ASCII header, then the ZIP.
        var header = TestData.Utf8("(DWF V06.20)");
        var zip = File.ReadAllBytes(TestData.WritePackage(".dwf", new Dictionary<string, byte[]> { ["a.txt"] = TestData.Utf8("x") }));
        var path = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N") + ".dwf");
        File.WriteAllBytes(path, header.Concat(zip).ToArray());
        try
        {
            DwfPackage.FindZipOffset(path).Should().Be(header.Length);
            FileKindDetector.IsZipBased(path).Should().BeTrue();
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void FindZipOffset_ReturnsNegative_WhenNoZipPresent()
    {
        var path = Path.GetTempFileName();
        File.WriteAllText(path, "this file has no zip container at all");
        try
        {
            DwfPackage.FindZipOffset(path).Should().BeLessThan(0);
            FileKindDetector.IsZipBased(path).Should().BeFalse();
        }
        finally { File.Delete(path); }
    }

    [Fact]
    public void OpenRead_ReadsEntries_ForPlainZipPackage()
    {
        var path = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["sheet.w2d"] = new byte[] { 1, 2, 3 },
            ["manifest.xml"] = TestData.Utf8("<m/>")
        });
        try
        {
            using var archive = DwfPackage.OpenRead(path);
            archive.Entries.Select(e => e.FullName).Should().Contain(new[] { "sheet.w2d", "manifest.xml" });
        }
        finally { File.Delete(path); }
    }
}
