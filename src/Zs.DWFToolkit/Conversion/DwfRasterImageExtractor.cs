using System.IO.Compression;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Conversion;

internal sealed class DwfRasterImageExtractor
{
    public async Task<IReadOnlyList<string>> ExtractAsync(
        string sourcePath,
        string outputDirectory,
        DwfRenderOptions options,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(sourcePath) || !FileKindDetector.IsZipBased(sourcePath))
            return Array.Empty<string>();

        SafePath.EnsureDirectory(outputDirectory);

        using var archive = ZipFile.OpenRead(sourcePath);
        var candidates = archive.Entries
            .Where(e => !string.IsNullOrEmpty(e.Name) && e.Length >= options.RasterImageMinBytes)
            .Select(e => new { Entry = e, Info = PackageEntryClassifier.ToInfo(e) })
            .Where(x => x.Info.IsImage || ImageProbe.IsLikelyImageExtension(x.Info.Extension))
            .OrderBy(x => ScoreImageEntry(x.Info))
            .ThenBy(x => x.Info.FullName, StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (options.SkipThumbnailWhenRasterImagesExist && candidates.Any(x => !x.Info.IsPossibleThumbnail))
            candidates = candidates.Where(x => !x.Info.IsPossibleThumbnail).ToList();

        var outputFiles = new List<string>();
        var seenHashes = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var index = 1;

        foreach (var candidate in candidates)
        {
            cancellationToken.ThrowIfCancellationRequested();

            byte[] bytes;
            await using (var input = candidate.Entry.Open())
            await using (var ms = new MemoryStream())
            {
                await input.CopyToAsync(ms, cancellationToken).ConfigureAwait(false);
                bytes = ms.ToArray();
            }

            var imageInfo = ImageProbe.TryProbe(bytes);
            if (imageInfo == null || imageInfo.Kind == EmbeddedImageKind.Unknown)
                continue;

            var hash = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes));
            if (!seenHashes.Add(hash))
                continue;

            var extension = imageInfo.Extension;
            var outputPath = Path.Combine(outputDirectory, $"page-{index:000}{extension}");
            await File.WriteAllBytesAsync(outputPath, bytes, cancellationToken).ConfigureAwait(false);
            outputFiles.Add(outputPath);
            index++;
        }

        return outputFiles;
    }

    private static int ScoreImageEntry(DwfPackageEntryInfo info)
    {
        var name = info.FullName.ToLowerInvariant();
        if (name.Contains("page") || name.Contains("sheet") || name.Contains("raster")) return 0;
        if (name.Contains("preview")) return 1;
        if (info.IsPossibleThumbnail) return 9;
        return 3;
    }
}
