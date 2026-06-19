using System.Text;
using FluentAssertions;
using Zs.DWFToolkit.Conversion;
using Zs.DWFToolkit.Models;
using Xunit;

namespace Zs.DWFToolkit.Tests.Conversion;

public sealed class SimpleImagePdfWriterTests
{
    [Fact]
    public void TryWrite_ProducesPdf_FromRgbPngAndJpeg()
    {
        var dir = Path.Combine(Path.GetTempPath(), "zs-pdf-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var png = Path.Combine(dir, "page-001.png");
        var jpg = Path.Combine(dir, "page-002.jpg");
        File.WriteAllBytes(png, TestData.BuildPng(100, 50, colorType: 2));
        File.WriteAllBytes(jpg, TestData.BuildJpeg(80, 60));
        var output = Path.Combine(dir, "out.pdf");

        try
        {
            var ok = SimpleImagePdfWriter.TryWrite(new[] { png, jpg }, output, new DwfRenderOptions(), out var message);

            ok.Should().BeTrue(message);
            File.Exists(output).Should().BeTrue();
            var text = Encoding.ASCII.GetString(File.ReadAllBytes(output));
            text.Should().StartWith("%PDF-1.4");
            text.Should().Contain("/Subtype /Image");
            text.Should().Contain("/DCTDecode");   // jpeg
            text.Should().Contain("/FlateDecode");  // png
            text.Should().Contain("startxref");
            text.Should().EndWith("%%EOF\n");
        }
        finally { Directory.Delete(dir, recursive: true); }
    }

    [Fact]
    public void TryWrite_Fails_WhenNoEmbeddableImages()
    {
        var output = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N") + ".pdf");

        var ok = SimpleImagePdfWriter.TryWrite(Array.Empty<string>(), output, new DwfRenderOptions(), out var message);

        ok.Should().BeFalse();
        message.Should().NotBeNullOrEmpty();
    }
}
