using System.IO.Compression;

namespace Zs.DWFToolkit.Internal;

/// <summary>
/// Opens the ZIP/OPC container inside a DWF/DWFx/XPS file, transparently skipping
/// the ASCII version header ("(DWF V06.20)") that classic DWF v6+ packages place
/// before the ZIP local-file signature.
/// </summary>
internal static class DwfPackage
{
    private static readonly byte[] ZipLocalFileSignature = [0x50, 0x4B, 0x03, 0x04]; // "PK\x03\x04"

    // The version header is tiny (e.g. "(DWF V06.20)"); scan a small window only.
    private const int MaxHeaderScanBytes = 64;

    /// <summary>Returns the byte offset of the embedded ZIP, or -1 when none is found.</summary>
    public static long FindZipOffset(string path)
    {
        using var stream = File.OpenRead(path);
        return FindZipOffset(stream);
    }

    public static long FindZipOffset(Stream stream)
    {
        var window = new byte[MaxHeaderScanBytes];
        var read = stream.Read(window, 0, window.Length);
        for (var i = 0; i + ZipLocalFileSignature.Length <= read; i++)
        {
            if (window[i] == ZipLocalFileSignature[0] &&
                window[i + 1] == ZipLocalFileSignature[1] &&
                window[i + 2] == ZipLocalFileSignature[2] &&
                window[i + 3] == ZipLocalFileSignature[3])
            {
                return i;
            }
        }
        return -1;
    }

    /// <summary>
    /// Opens the package as a <see cref="ZipArchive"/>. Throws <see cref="InvalidDataException"/>
    /// when the file contains no ZIP container. The whole file (including any classic
    /// DWF version header) is handed to <see cref="ZipArchive"/>: the header is data
    /// before the first local entry, and the ZIP central-directory offsets are
    /// absolute from the start of the file, so the reader locates entries correctly.
    /// </summary>
    public static ZipArchive OpenRead(string path)
    {
        if (FindZipOffset(path) < 0)
            throw new InvalidDataException("File does not contain a ZIP/OPC container.");

        return new ZipArchive(File.OpenRead(path), ZipArchiveMode.Read, leaveOpen: false);
    }
}
