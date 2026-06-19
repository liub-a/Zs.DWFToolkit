using System.IO.Compression;

namespace Zs.DWFToolkit.Tests;

/// <summary>
/// Builds minimal binary fixtures (images, OPC packages) for tests without relying
/// on a platform image codec. PNG/JPEG headers are hand-built; chunk CRCs are zeroed
/// because the toolkit's probes parse structure only and never validate CRC.
/// </summary>
internal static class TestData
{
    public static byte[] BuildPng(int width, int height, byte colorType = 2)
    {
        using var ms = new MemoryStream();
        ms.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 });

        var ihdr = new byte[13];
        WriteBe(ihdr, 0, width);
        WriteBe(ihdr, 4, height);
        ihdr[8] = 8;          // bit depth
        ihdr[9] = colorType;  // 2 = RGB, 0 = gray
        ihdr[10] = 0;         // compression
        ihdr[11] = 0;         // filter
        ihdr[12] = 0;         // interlace
        WriteChunk(ms, "IHDR", ihdr);

        // IDAT content does not need to decode for structural tests.
        WriteChunk(ms, "IDAT", new byte[] { 0x78, 0x9C, 0x01, 0x00 });
        WriteChunk(ms, "IEND", Array.Empty<byte>());
        return ms.ToArray();
    }

    public static byte[] BuildJpeg(int width, int height)
    {
        using var ms = new MemoryStream();
        ms.Write(new byte[] { 0xFF, 0xD8 });            // SOI
        // SOF0 marker: length(17) precision(8) height width components(3)
        ms.Write(new byte[] { 0xFF, 0xC0, 0x00, 0x11, 0x08 });
        ms.WriteByte((byte)(height >> 8));
        ms.WriteByte((byte)(height & 0xFF));
        ms.WriteByte((byte)(width >> 8));
        ms.WriteByte((byte)(width & 0xFF));
        ms.WriteByte(3);
        ms.Write(new byte[] { 1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0 });
        ms.Write(new byte[] { 0xFF, 0xD9 });            // EOI
        return ms.ToArray();
    }

    public static byte[] BuildBmp(int width, int height)
    {
        var bmp = new byte[54];
        bmp[0] = (byte)'B';
        bmp[1] = (byte)'M';
        BitConverter.GetBytes(width).CopyTo(bmp, 18);
        BitConverter.GetBytes(height).CopyTo(bmp, 22);
        return bmp;
    }

    /// <summary>Writes a ZIP package to a temp file with the given extension and entries.</summary>
    public static string WritePackage(string extension, IReadOnlyDictionary<string, byte[]> entries)
    {
        var path = Path.Combine(Path.GetTempPath(), "zs-dwf-test-" + Guid.NewGuid().ToString("N") + extension);
        using var fs = File.Create(path);
        using var zip = new ZipArchive(fs, ZipArchiveMode.Create);
        foreach (var (name, bytes) in entries)
        {
            var entry = zip.CreateEntry(name);
            using var s = entry.Open();
            s.Write(bytes);
        }
        return path;
    }

    public static byte[] Utf8(string text) => System.Text.Encoding.UTF8.GetBytes(text);

    private static void WriteChunk(Stream stream, string type, byte[] data)
    {
        var len = new byte[4];
        WriteBe(len, 0, data.Length);
        stream.Write(len);
        stream.Write(System.Text.Encoding.ASCII.GetBytes(type));
        stream.Write(data);
        stream.Write(new byte[4]); // zeroed CRC
    }

    private static void WriteBe(byte[] buffer, int offset, int value)
    {
        buffer[offset] = (byte)(value >> 24);
        buffer[offset + 1] = (byte)(value >> 16);
        buffer[offset + 2] = (byte)(value >> 8);
        buffer[offset + 3] = (byte)value;
    }
}
