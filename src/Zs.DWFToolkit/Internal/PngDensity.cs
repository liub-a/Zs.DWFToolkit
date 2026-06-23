using System.Buffers.Binary;

namespace Zs.DWFToolkit.Internal;

/// <summary>
/// Injects a PNG <c>pHYs</c> (physical pixel dimensions) chunk so downstream assemblers such as
/// <c>mutool convert</c> size the PDF page from the real DPI instead of defaulting to 96.
/// Pure byte manipulation — no image decoding.
/// </summary>
internal static class PngDensity
{
    private static readonly byte[] Signature = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

    /// <summary>Sets the PNG's pixels-per-metre density to match <paramref name="dpi"/>. Returns false for non-PNG or malformed data.</summary>
    public static bool TrySetDpi(string pngPath, int dpi)
    {
        if (dpi <= 0 || !File.Exists(pngPath)) return false;

        byte[] bytes;
        try { bytes = File.ReadAllBytes(pngPath); }
        catch { return false; }

        if (bytes.Length < Signature.Length || !bytes.AsSpan(0, Signature.Length).SequenceEqual(Signature))
            return false;

        // First chunk must be IHDR; pHYs is inserted immediately after it.
        var ihdrStart = Signature.Length;
        if (ihdrStart + 8 > bytes.Length) return false;
        var ihdrLen = BinaryPrimitives.ReadInt32BigEndian(bytes.AsSpan(ihdrStart, 4));
        if (ReadType(bytes, ihdrStart + 4) != "IHDR") return false;
        var afterIhdr = ihdrStart + 8 + ihdrLen + 4; // length + type + data + CRC
        if (afterIhdr > bytes.Length) return false;

        var ppu = (uint)Math.Round(dpi / 0.0254); // pixels per metre
        var physChunk = BuildPhysChunk(ppu);

        using var fs = File.Create(pngPath);
        fs.Write(bytes, 0, afterIhdr);

        // Skip an existing pHYs chunk so we replace rather than duplicate it.
        var rest = afterIhdr;
        if (rest + 8 <= bytes.Length && ReadType(bytes, rest + 4) == "pHYs")
        {
            var len = BinaryPrimitives.ReadInt32BigEndian(bytes.AsSpan(rest, 4));
            rest += 8 + len + 4;
        }

        fs.Write(physChunk, 0, physChunk.Length);
        fs.Write(bytes, rest, bytes.Length - rest);
        return true;
    }

    private static byte[] BuildPhysChunk(uint ppu)
    {
        var chunk = new byte[8 + 9 + 4];
        BinaryPrimitives.WriteInt32BigEndian(chunk.AsSpan(0, 4), 9); // data length
        chunk[4] = (byte)'p'; chunk[5] = (byte)'H'; chunk[6] = (byte)'Y'; chunk[7] = (byte)'s';
        BinaryPrimitives.WriteUInt32BigEndian(chunk.AsSpan(8, 4), ppu);
        BinaryPrimitives.WriteUInt32BigEndian(chunk.AsSpan(12, 4), ppu);
        chunk[16] = 1; // unit specifier: metres
        var crc = Crc32(chunk, 4, 13); // CRC over type + data
        BinaryPrimitives.WriteUInt32BigEndian(chunk.AsSpan(17, 4), crc);
        return chunk;
    }

    private static string ReadType(byte[] b, int offset) =>
        offset + 4 <= b.Length ? System.Text.Encoding.ASCII.GetString(b, offset, 4) : string.Empty;

    private static readonly uint[] CrcTable = BuildCrcTable();

    private static uint[] BuildCrcTable()
    {
        var table = new uint[256];
        for (uint n = 0; n < 256; n++)
        {
            var c = n;
            for (var k = 0; k < 8; k++)
                c = (c & 1) != 0 ? 0xEDB88320 ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        return table;
    }

    private static uint Crc32(byte[] data, int offset, int length)
    {
        var c = 0xFFFFFFFFu;
        for (var i = offset; i < offset + length; i++)
            c = CrcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }
}
