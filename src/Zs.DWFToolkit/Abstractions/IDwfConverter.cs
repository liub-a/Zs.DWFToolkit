using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

public interface IDwfConverter
{
    Task<DwfRenderResult> ConvertToPdfAsync(
        string sourcePath,
        string outputPdfPath,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default);

    Task<DwfRenderResult> ConvertToImagesAsync(
        string sourcePath,
        string outputDirectory,
        DwfRenderOptions? options = null,
        CancellationToken cancellationToken = default);
}
