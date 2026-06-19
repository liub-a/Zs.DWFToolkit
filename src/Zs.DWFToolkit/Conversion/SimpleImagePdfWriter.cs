using System.Globalization;
using System.Text;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Conversion;

internal static class SimpleImagePdfWriter
{
    public static bool TryWrite(
        IReadOnlyList<string> imagePaths,
        string outputPdfPath,
        DwfRenderOptions options,
        out string? message)
    {
        message = null;
        var images = new List<PdfImage>();

        foreach (var path in imagePaths)
        {
            if (!File.Exists(path)) continue;
            var bytes = File.ReadAllBytes(path);
            var info = ImageProbe.TryProbe(bytes);
            if (info is not { CanEmbedInSimplePdf: true } || info.Width <= 0 || info.Height <= 0)
            {
                message = $"Simple PDF writer can embed JPEG and non-alpha RGB/gray PNG only. Unsupported image: {path}";
                return false;
            }

            byte[] streamBytes;
            string filter;
            string? decodeParms = null;

            if (info.Kind == EmbeddedImageKind.Jpeg)
            {
                streamBytes = bytes;
                filter = "/DCTDecode";
            }
            else if (info.Kind == EmbeddedImageKind.Png && ImageProbe.TryReadPngIdat(bytes, out var idat))
            {
                streamBytes = idat;
                filter = "/FlateDecode";
                decodeParms = info.PdfDecodeParms;
            }
            else
            {
                message = $"Unable to extract image stream for PDF: {path}";
                return false;
            }

            images.Add(new PdfImage(path, info, streamBytes, filter, decodeParms));
        }

        if (images.Count == 0)
        {
            message = "No embeddable images were supplied.";
            return false;
        }

        try
        {
            SafePath.EnsureParentDirectory(outputPdfPath);
            using var fs = File.Create(outputPdfPath);
            var offsets = new List<long> { 0 };
            WriteAscii(fs, "%PDF-1.4\n%\u00E2\u00E3\u00CF\u00D3\n");

            var catalogId = 1;
            var pagesId = 2;
            var pageIds = new List<int>();
            var imageIds = new List<int>();
            var nextId = 3;
            foreach (var _ in images)
            {
                pageIds.Add(nextId++);
                imageIds.Add(nextId++);
            }

            WriteObject(fs, offsets, catalogId, $"<< /Type /Catalog /Pages {pagesId} 0 R >>\n");
            WriteObject(fs, offsets, pagesId, $"<< /Type /Pages /Count {pageIds.Count} /Kids [ {string.Join(" ", pageIds.Select(id => $"{id} 0 R"))} ] >>\n");

            for (var i = 0; i < images.Count; i++)
            {
                var img = images[i];
                var (pageW, pageH, drawW, drawH, drawX, drawY) = ComputePageAndPlacement(img.Info, options);
                var content = $"q\n{F(drawW)} 0 0 {F(drawH)} {F(drawX)} {F(drawY)} cm\n/Im{i + 1} Do\nQ\n";
                var contentBytes = Encoding.ASCII.GetBytes(content);
                var contentId = nextId++;

                var pageDict = $"<< /Type /Page /Parent {pagesId} 0 R /MediaBox [0 0 {F(pageW)} {F(pageH)}] /Resources << /XObject << /Im{i + 1} {imageIds[i]} 0 R >> >> /Contents {contentId} 0 R >>\n";
                WriteObject(fs, offsets, pageIds[i], pageDict);

                var colorSpace = img.Info.PdfColorSpace ?? "/DeviceRGB";
                var imageDict = new StringBuilder();
                imageDict.Append($"<< /Type /XObject /Subtype /Image /Width {img.Info.Width} /Height {img.Info.Height} /ColorSpace {colorSpace} /BitsPerComponent {img.Info.BitsPerComponent} /Filter {img.Filter} ");
                if (!string.IsNullOrWhiteSpace(img.DecodeParms))
                    imageDict.Append($"/DecodeParms << {img.DecodeParms} >> ");
                imageDict.Append($"/Length {img.StreamBytes.Length} >>\nstream\n");
                WriteStreamObject(fs, offsets, imageIds[i], imageDict.ToString(), img.StreamBytes);

                WriteStreamObject(fs, offsets, contentId, $"<< /Length {contentBytes.Length} >>\nstream\n", contentBytes);
            }

            var xrefStart = fs.Position;
            WriteAscii(fs, $"xref\n0 {offsets.Count}\n0000000000 65535 f \n");
            for (var i = 1; i < offsets.Count; i++)
                WriteAscii(fs, $"{offsets[i]:0000000000} 00000 n \n");

            WriteAscii(fs, $"trailer\n<< /Size {offsets.Count} /Root {catalogId} 0 R >>\nstartxref\n{xrefStart}\n%%EOF\n");
            return true;
        }
        catch (Exception ex)
        {
            message = ex.Message;
            return false;
        }
    }

    private static (double PageW, double PageH, double DrawW, double DrawH, double DrawX, double DrawY) ComputePageAndPlacement(EmbeddedImageInfo info, DwfRenderOptions options)
    {
        var margin = Math.Max(0, options.PdfMarginPoints);
        var naturalW = info.Width * 72.0 / Math.Max(1, options.Dpi);
        var naturalH = info.Height * 72.0 / Math.Max(1, options.Dpi);
        var pageW = options.PdfPageWidthPoints ?? naturalW + margin * 2;
        var pageH = options.PdfPageHeightPoints ?? naturalH + margin * 2;
        var maxW = Math.Max(1, pageW - margin * 2);
        var maxH = Math.Max(1, pageH - margin * 2);
        var scale = Math.Min(maxW / naturalW, maxH / naturalH);
        if (double.IsNaN(scale) || double.IsInfinity(scale) || scale <= 0) scale = 1;
        var drawW = naturalW * scale;
        var drawH = naturalH * scale;
        var drawX = (pageW - drawW) / 2.0;
        var drawY = (pageH - drawH) / 2.0;
        return (pageW, pageH, drawW, drawH, drawX, drawY);
    }

    private static void WriteObject(Stream stream, List<long> offsets, int id, string body)
    {
        EnsureOffsetSlot(offsets, id);
        offsets[id] = stream.Position;
        WriteAscii(stream, $"{id} 0 obj\n");
        WriteAscii(stream, body);
        WriteAscii(stream, "endobj\n");
    }

    private static void WriteStreamObject(Stream stream, List<long> offsets, int id, string header, byte[] bytes)
    {
        EnsureOffsetSlot(offsets, id);
        offsets[id] = stream.Position;
        WriteAscii(stream, $"{id} 0 obj\n");
        WriteAscii(stream, header);
        stream.Write(bytes, 0, bytes.Length);
        WriteAscii(stream, "\nendstream\nendobj\n");
    }

    private static void EnsureOffsetSlot(List<long> offsets, int id)
    {
        while (offsets.Count <= id) offsets.Add(0);
    }

    private static void WriteAscii(Stream stream, string text)
    {
        var bytes = Encoding.ASCII.GetBytes(text);
        stream.Write(bytes, 0, bytes.Length);
    }

    private static string F(double value) => value.ToString("0.###", CultureInfo.InvariantCulture);

    private sealed record PdfImage(string Path, EmbeddedImageInfo Info, byte[] StreamBytes, string Filter, string? DecodeParms);
}
