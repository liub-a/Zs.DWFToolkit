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

    /// <summary>
    /// Renders every 2D page of a DWF/DWFx (or a bare W2D) to a single true-vector
    /// PDF — drawing operators, not rasterized images — so output is compact and
    /// resolution-independent. Requires the native renderer build.
    /// </summary>
    Task<DwfRenderResult> RenderDwfToPdfAsync(
        string sourcePath,
        string outputPdfPath,
        DwfRenderOptions options,
        CancellationToken cancellationToken = default);
}
