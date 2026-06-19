using System.IO.Compression;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Internal;

internal static class FileKindDetector
{
    public static bool IsZipBased(string path)
    {
        // Classic DWF v6+ packages prefix the ZIP with an ASCII "(DWF V06.20)"
        // header, so the PK signature is not always at byte 0.
        return DwfPackage.FindZipOffset(path) >= 0;
    }

    public static DwfFileKind Detect(string path)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        if (ext == ".dwfx") return DwfFileKind.Dwfx;
        if (ext == ".xps") return DwfFileKind.Xps;
        if (ext == ".dwf") return DwfFileKind.Dwf;
        return DwfFileKind.Unknown;
    }

    public static DwfFileKind DetectFromZipEntries(string path, DwfFileKind extensionKind)
    {
        if (!IsZipBased(path)) return extensionKind;
        try
        {
            using var archive = DwfPackage.OpenRead(path);
            var names = archive.Entries.Select(e => e.FullName.Replace('\\', '/')).ToArray();
            if (names.Any(n => n.Equals("[Content_Types].xml", StringComparison.OrdinalIgnoreCase)))
            {
                if (extensionKind == DwfFileKind.Dwfx || names.Any(n => n.EndsWith(".fpage", StringComparison.OrdinalIgnoreCase)))
                    return extensionKind == DwfFileKind.Xps ? DwfFileKind.Xps : DwfFileKind.Dwfx;
            }
            if (names.Any(n => n.EndsWith(".w2d", StringComparison.OrdinalIgnoreCase)))
                return DwfFileKind.Dwf;
        }
        catch
        {
            return extensionKind;
        }
        return extensionKind;
    }
}
