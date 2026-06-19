using System.IO.Compression;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Internal;

internal static class PackageEntryClassifier
{
    private static readonly HashSet<string> ImageExts = new(StringComparer.OrdinalIgnoreCase)
    {
        ".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp"
    };

    public static DwfPackageEntryInfo ToInfo(ZipArchiveEntry entry)
    {
        var name = entry.FullName.Replace('\\', '/');
        var ext = Path.GetExtension(name);
        var lower = name.ToLowerInvariant();
        var isImage = ImageExts.Contains(ext);
        var isXml = string.Equals(ext, ".xml", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(ext, ".rels", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(ext, ".fdseq", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(ext, ".fdoc", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(ext, ".fpage", StringComparison.OrdinalIgnoreCase);

        return new DwfPackageEntryInfo(
            FullName: name,
            Length: entry.Length,
            Extension: ext,
            IsImage: isImage,
            IsXml: isXml,
            IsPossibleThumbnail: isImage && (lower.Contains("thumb") || lower.Contains("preview") || lower.Contains("thumbnail")),
            IsPossibleW2d: string.Equals(ext, ".w2d", StringComparison.OrdinalIgnoreCase),
            IsPossibleXpsPage: string.Equals(ext, ".fpage", StringComparison.OrdinalIgnoreCase));
    }
}
