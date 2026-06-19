using FluentAssertions;
using Zs.DWFToolkit.Internal;
using Xunit;

namespace Zs.DWFToolkit.Tests.Internal;

public sealed class ImageProbeTests
{
    [Fact]
    public void DetectKind_ReturnsPng_ForPngSignature()
    {
        var bytes = TestData.BuildPng(10, 20);

        ImageProbe.DetectKind(bytes).Should().Be(EmbeddedImageKind.Png);
    }

    [Fact]
    public void DetectKind_ReturnsJpeg_ForJpegSignature()
    {
        ImageProbe.DetectKind(TestData.BuildJpeg(8, 8)).Should().Be(EmbeddedImageKind.Jpeg);
    }

    [Fact]
    public void DetectKind_ReturnsUnknown_ForArbitraryBytes()
    {
        ImageProbe.DetectKind(new byte[] { 1, 2, 3, 4 }).Should().Be(EmbeddedImageKind.Unknown);
    }

    [Fact]
    public void TryProbe_ReadsPngDimensions_AndMarksRgbEmbeddable()
    {
        var info = ImageProbe.TryProbe(TestData.BuildPng(64, 48, colorType: 2));

        info.Should().NotBeNull();
        info!.Width.Should().Be(64);
        info.Height.Should().Be(48);
        info.CanEmbedInSimplePdf.Should().BeTrue();
        info.PdfColorSpace.Should().Be("/DeviceRGB");
    }

    [Fact]
    public void TryProbe_ReadsJpegDimensionsFromSof()
    {
        var info = ImageProbe.TryProbe(TestData.BuildJpeg(120, 90));

        info.Should().NotBeNull();
        info!.Kind.Should().Be(EmbeddedImageKind.Jpeg);
        info.Width.Should().Be(120);
        info.Height.Should().Be(90);
    }

    [Fact]
    public void TryProbe_ReadsBmpDimensions()
    {
        var info = ImageProbe.TryProbe(TestData.BuildBmp(33, 17));

        info.Should().NotBeNull();
        info!.Kind.Should().Be(EmbeddedImageKind.Bmp);
        info.Width.Should().Be(33);
        info.Height.Should().Be(17);
    }

    [Theory]
    [InlineData(".png", true)]
    [InlineData(".JPG", true)]
    [InlineData(".w2d", false)]
    [InlineData(".txt", false)]
    public void IsLikelyImageExtension_MatchesKnownImageExtensions(string ext, bool expected)
    {
        ImageProbe.IsLikelyImageExtension(ext).Should().Be(expected);
    }

    [Fact]
    public void TryReadPngIdat_ExtractsIdatBytes()
    {
        var ok = ImageProbe.TryReadPngIdat(TestData.BuildPng(4, 4), out var idat);

        ok.Should().BeTrue();
        idat.Should().NotBeEmpty();
    }
}
