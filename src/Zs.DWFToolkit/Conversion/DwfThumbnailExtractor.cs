using System.IO.Compression;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Internal;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Conversion;

public sealed class DwfThumbnailExtractor : IDwfThumbnailExtractor
{
    public async Task<DwfThumbnailResult> ExtractBestThumbnailAsync(
        string sourcePath,
        string outputPath,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(sourcePath))
        {
            return new DwfThumbnailResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.FileNotFound,
                ErrorMessage = "File not found."
            };
        }

        if (!FileKindDetector.IsZipBased(sourcePath))
        {
            return new DwfThumbnailResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.UnsupportedFormat,
                ErrorMessage = "Only ZIP/OPC style DWF/DWFx packages are supported by the thumbnail extractor."
            };
        }

        using var archive = ZipFile.OpenRead(sourcePath);
        var candidates = archive.Entries
            .Where(e => !string.IsNullOrEmpty(e.Name))
            .Select(e => new { Entry = e, Info = PackageEntryClassifier.ToInfo(e) })
            .Where(x => x.Info.IsImage)
            .OrderByDescending(x => x.Info.IsPossibleThumbnail)
            .ThenByDescending(x => x.Entry.Length)
            .ToList();

        var best = candidates.FirstOrDefault();
        if (best == null)
        {
            return new DwfThumbnailResult
            {
                Success = false,
                ErrorCode = DwfErrorCodes.NoThumbnail,
                ErrorMessage = "No embedded image/thumbnail resource was found."
            };
        }

        SafePath.EnsureParentDirectory(outputPath);

        await using var input = best.Entry.Open();
        await using var output = File.Create(outputPath);
        await input.CopyToAsync(output, cancellationToken).ConfigureAwait(false);

        return new DwfThumbnailResult
        {
            Success = true,
            ErrorCode = DwfErrorCodes.Ok,
            ResourcePath = best.Info.FullName,
            OutputPath = outputPath,
            BytesWritten = output.Length
        };
    }
}
