using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

public interface IDwfThumbnailExtractor
{
    Task<DwfThumbnailResult> ExtractBestThumbnailAsync(
        string sourcePath,
        string outputPath,
        CancellationToken cancellationToken = default);
}
