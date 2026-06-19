using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

public interface INativeDwfRenderer
{
    bool IsAvailable { get; }

    Task<DwfDocumentInfo> GetInfoAsync(string sourcePath, CancellationToken cancellationToken = default);

    Task<DwfRenderResult> RenderPageAsync(
        string sourcePath,
        int pageIndex,
        string outputPath,
        DwfRenderOptions options,
        CancellationToken cancellationToken = default);

    Task<DwfRenderResult> RenderW2dFileAsync(
        string sourcePath,
        string outputPath,
        DwfRenderOptions options,
        CancellationToken cancellationToken = default);
}
