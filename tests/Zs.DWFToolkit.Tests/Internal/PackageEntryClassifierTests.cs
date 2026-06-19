using System.IO.Compression;
using FluentAssertions;
using Zs.DWFToolkit.Internal;
using Xunit;

namespace Zs.DWFToolkit.Tests.Internal;

public sealed class PackageEntryClassifierTests
{
    private static (string Path, ZipArchive Archive) Open(IReadOnlyDictionary<string, byte[]> entries)
    {
        var path = TestData.WritePackage(".dwf", entries);
        return (path, ZipFile.OpenRead(path));
    }

    [Fact]
    public void ToInfo_FlagsThumbnailImage()
    {
        var (path, archive) = Open(new Dictionary<string, byte[]> { ["preview/thumbnail.png"] = TestData.BuildPng(2, 2) });
        try
        {
            var info = PackageEntryClassifier.ToInfo(archive.GetEntry("preview/thumbnail.png")!);
            info.IsImage.Should().BeTrue();
            info.IsPossibleThumbnail.Should().BeTrue();
        }
        finally { archive.Dispose(); File.Delete(path); }
    }

    [Fact]
    public void ToInfo_FlagsW2dResource()
    {
        var (path, archive) = Open(new Dictionary<string, byte[]> { ["sheet.w2d"] = new byte[] { 1 } });
        try
        {
            var info = PackageEntryClassifier.ToInfo(archive.GetEntry("sheet.w2d")!);
            info.IsPossibleW2d.Should().BeTrue();
            info.IsPossibleW3d.Should().BeFalse();
        }
        finally { archive.Dispose(); File.Delete(path); }
    }

    [Fact]
    public void ToInfo_FlagsW3dResource()
    {
        var (path, archive) = Open(new Dictionary<string, byte[]> { ["model.w3d"] = new byte[] { 1 } });
        try
        {
            var info = PackageEntryClassifier.ToInfo(archive.GetEntry("model.w3d")!);
            info.IsPossibleW3d.Should().BeTrue();
        }
        finally { archive.Dispose(); File.Delete(path); }
    }

    [Fact]
    public void ToInfo_FlagsFixedPage()
    {
        var (path, archive) = Open(new Dictionary<string, byte[]> { ["p/1.fpage"] = TestData.Utf8("<FixedPage/>") });
        try
        {
            var info = PackageEntryClassifier.ToInfo(archive.GetEntry("p/1.fpage")!);
            info.IsPossibleXpsPage.Should().BeTrue();
        }
        finally { archive.Dispose(); File.Delete(path); }
    }
}
