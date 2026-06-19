namespace Zs.DWFToolkit.Internal;

internal enum EmbeddedImageKind
{
    Unknown = 0,
    Jpeg = 1,
    Png = 2,
    Bmp = 3,
    Tiff = 4,
    Webp = 5
}

internal sealed record EmbeddedImageInfo(
    EmbeddedImageKind Kind,
    int Width,
    int Height,
    int BitsPerComponent,
    int Components,
    string Extension,
    string? PdfColorSpace = null,
    bool CanEmbedInSimplePdf = false,
    string? PdfDecodeParms = null);

internal static class ImageProbe
{
    private static readonly byte[] PngSignature = [137, 80, 78, 71, 13, 10, 26, 10];

    public static EmbeddedImageKind DetectKind(ReadOnlySpan<byte> data)
    {
        if (data.Length >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return EmbeddedImageKind.Jpeg;
        if (data.Length >= 8 && data[..8].SequenceEqual(PngSignature)) return EmbeddedImageKind.Png;
        if (data.Length >= 2 && data[0] == 'B' && data[1] == 'M') return EmbeddedImageKind.Bmp;
        if (data.Length >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) || (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42))) return EmbeddedImageKind.Tiff;
        if (data.Length >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' && data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') return EmbeddedImageKind.Webp;
        return EmbeddedImageKind.Unknown;
    }

    public static string ExtensionFor(EmbeddedImageKind kind) => kind switch
    {
        EmbeddedImageKind.Jpeg => ".jpg",
        EmbeddedImageKind.Png => ".png",
        EmbeddedImageKind.Bmp => ".bmp",
        EmbeddedImageKind.Tiff => ".tif",
        EmbeddedImageKind.Webp => ".webp",
        _ => ".bin"
    };

    public static EmbeddedImageInfo? TryProbe(byte[] data)
    {
        var kind = DetectKind(data);
        return kind switch
        {
            EmbeddedImageKind.Jpeg => TryProbeJpeg(data),
            EmbeddedImageKind.Png => TryProbePng(data),
            EmbeddedImageKind.Bmp => TryProbeBmp(data),
            _ => kind == EmbeddedImageKind.Unknown ? null : new EmbeddedImageInfo(kind, 0, 0, 0, 0, ExtensionFor(kind))
        };
    }

    public static EmbeddedImageInfo? TryProbeFile(string path)
    {
        try
        {
            var data = File.ReadAllBytes(path);
            return TryProbe(data);
        }
        catch
        {
            return null;
        }
    }

    public static bool IsLikelyImageExtension(string extension)
    {
        return extension.Equals(".png", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".jpg", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".jpeg", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".bmp", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".tif", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".tiff", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".webp", StringComparison.OrdinalIgnoreCase);
    }

    private static EmbeddedImageInfo? TryProbeJpeg(byte[] data)
    {
        if (data.Length < 4) return null;
        var i = 2;
        while (i + 9 < data.Length)
        {
            if (data[i] != 0xFF)
            {
                i++;
                continue;
            }

            while (i < data.Length && data[i] == 0xFF) i++;
            if (i >= data.Length) break;

            var marker = data[i++];
            if (marker is 0xD8 or 0xD9 or 0x01 || marker is >= 0xD0 and <= 0xD7)
                continue;

            if (i + 1 >= data.Length) break;
            var length = (data[i] << 8) + data[i + 1];
            if (length < 2 || i + length > data.Length) break;

            if (IsSofMarker(marker) && length >= 8)
            {
                var bits = data[i + 2];
                var height = (data[i + 3] << 8) + data[i + 4];
                var width = (data[i + 5] << 8) + data[i + 6];
                var components = data[i + 7];
                var colorSpace = components switch
                {
                    1 => "/DeviceGray",
                    3 => "/DeviceRGB",
                    4 => "/DeviceCMYK",
                    _ => "/DeviceRGB"
                };
                return new EmbeddedImageInfo(EmbeddedImageKind.Jpeg, width, height, bits, components, ".jpg", colorSpace, CanEmbedInSimplePdf: width > 0 && height > 0);
            }

            i += length;
        }

        return new EmbeddedImageInfo(EmbeddedImageKind.Jpeg, 0, 0, 8, 3, ".jpg", "/DeviceRGB", CanEmbedInSimplePdf: false);
    }

    private static bool IsSofMarker(byte marker)
    {
        return marker is 0xC0 or 0xC1 or 0xC2 or 0xC3 or 0xC5 or 0xC6 or 0xC7 or 0xC9 or 0xCA or 0xCB or 0xCD or 0xCE or 0xCF;
    }

    private static EmbeddedImageInfo? TryProbePng(byte[] data)
    {
        if (data.Length < 33 || !data.AsSpan(0, 8).SequenceEqual(PngSignature)) return null;
        var pos = 8;
        if (!TryReadPngChunkHeader(data, ref pos, out var length, out var type) || type != "IHDR" || length < 13) return null;
        if (pos + length + 4 > data.Length) return null;

        var width = ReadInt32BigEndian(data, pos);
        var height = ReadInt32BigEndian(data, pos + 4);
        var bitDepth = data[pos + 8];
        var colorType = data[pos + 9];
        var compression = data[pos + 10];
        var filter = data[pos + 11];
        var interlace = data[pos + 12];

        var canEmbed = bitDepth == 8 && compression == 0 && filter == 0 && interlace == 0 && colorType is 0 or 2;
        var components = colorType switch
        {
            0 => 1,
            2 => 3,
            3 => 1,
            4 => 2,
            6 => 4,
            _ => 0
        };
        var colorSpace = colorType == 0 ? "/DeviceGray" : "/DeviceRGB";
        var decodeParms = canEmbed
            ? $"/Predictor 15 /Colors {components} /BitsPerComponent {bitDepth} /Columns {width}"
            : null;

        return new EmbeddedImageInfo(EmbeddedImageKind.Png, width, height, bitDepth, components, ".png", colorSpace, canEmbed, decodeParms);
    }

    private static EmbeddedImageInfo? TryProbeBmp(byte[] data)
    {
        if (data.Length < 26 || data[0] != 'B' || data[1] != 'M') return null;
        var width = BitConverter.ToInt32(data, 18);
        var height = Math.Abs(BitConverter.ToInt32(data, 22));
        return new EmbeddedImageInfo(EmbeddedImageKind.Bmp, width, height, 0, 0, ".bmp");
    }

    internal static bool TryReadPngIdat(byte[] data, out byte[] compressedIdat)
    {
        compressedIdat = [];
        if (data.Length < 33 || !data.AsSpan(0, 8).SequenceEqual(PngSignature)) return false;

        using var ms = new MemoryStream();
        var pos = 8;
        while (TryReadPngChunkHeader(data, ref pos, out var length, out var type))
        {
            if (length < 0 || pos + length + 4 > data.Length) return false;
            if (type == "IDAT") ms.Write(data, pos, length);
            pos += length + 4; // skip data + CRC
            if (type == "IEND") break;
        }

        compressedIdat = ms.ToArray();
        return compressedIdat.Length > 0;
    }

    private static bool TryReadPngChunkHeader(byte[] data, ref int pos, out int length, out string type)
    {
        length = 0;
        type = string.Empty;
        if (pos + 8 > data.Length) return false;
        length = ReadInt32BigEndian(data, pos);
        type = System.Text.Encoding.ASCII.GetString(data, pos + 4, 4);
        pos += 8;
        return true;
    }

    private static int ReadInt32BigEndian(byte[] data, int offset)
    {
        return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
    }
}
