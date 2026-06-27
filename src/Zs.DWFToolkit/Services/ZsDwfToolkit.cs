using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Conversion;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Native;

namespace Zs.DWFToolkit.Services;

/// <summary>
/// High-level facade for common DWF/DWFx operations.
/// </summary>
public sealed class ZsDwfToolkit
{
    private readonly IDwfDocumentReader _reader;
    private readonly IDwfThumbnailExtractor _thumbnailExtractor;
    private readonly IDwfConverter _converter;
    private readonly IDwfEditor _editor;

    public ZsDwfToolkit(
        IDwfDocumentReader? reader = null,
        IDwfThumbnailExtractor? thumbnailExtractor = null,
        IDwfConverter? converter = null,
        IDwfEditor? editor = null)
    {
        _reader = reader ?? new DwfDocumentReader(new NativeDwfRenderer());
        _thumbnailExtractor = thumbnailExtractor ?? new DwfThumbnailExtractor();
        _converter = converter ?? new DwfxExternalConverter(_reader, _thumbnailExtractor, new NativeDwfRenderer());
        _editor = editor ?? new NativeDwfEditor();
    }

    public Task<DwfDocumentInfo> ReadInfoAsync(string sourcePath, CancellationToken cancellationToken = default)
        => _reader.ReadInfoAsync(sourcePath, cancellationToken);

    public Task<DwfThumbnailResult> ExtractBestThumbnailAsync(string sourcePath, string outputPath, CancellationToken cancellationToken = default)
        => _thumbnailExtractor.ExtractBestThumbnailAsync(sourcePath, outputPath, cancellationToken);

    public Task<DwfRenderResult> ConvertToPdfAsync(string sourcePath, string outputPdfPath, DwfRenderOptions? options = null, CancellationToken cancellationToken = default)
        => _converter.ConvertToPdfAsync(sourcePath, outputPdfPath, options, cancellationToken);

    public Task<DwfRenderResult> ConvertToImagesAsync(string sourcePath, string outputDirectory, DwfRenderOptions? options = null, CancellationToken cancellationToken = default)
        => _converter.ConvertToImagesAsync(sourcePath, outputDirectory, options, cancellationToken);

    /// <summary>Whether native write-back editing (stamping) is available.</summary>
    public bool CanEdit => _editor.IsAvailable;

    /// <summary>
    /// Stamps a raw RGBA image onto a W2D stream at the given logical rectangle,
    /// writing a new W2D file. Requires the native renderer build.
    /// </summary>
    public Task<DwfRenderResult> StampImageOnW2dAsync(
        string inputW2dPath,
        string outputW2dPath,
        byte[] rgbaPixels,
        int imageWidth,
        int imageHeight,
        DwfStampRect rect,
        CancellationToken cancellationToken = default)
        => _editor.StampImageOnW2dAsync(inputW2dPath, outputW2dPath, rgbaPixels, imageWidth, imageHeight, rect, cancellationToken);

    /// <summary>
    /// 包级写回签署：把多个印章（按右下角锚点的毫米位置）贴进 DWF 内每页 W2D，输出合法签署 DWF。
    /// 自动完成 W2D 坐标换算、多页分发、DWF 重打包（含 (DWF V06.00) 前缀与 ZIP 偏移修正）。
    /// 图幅尺寸/视图等头部元数据原样保留，外部阅读器与本库渲染均保持正确纵横比。
    /// </summary>
    public Task<DwfRenderResult> StampImagesOnDwfAsync(
        string inputDwfPath,
        string outputDwfPath,
        IReadOnlyList<DwfImageStamp> stamps,
        CancellationToken cancellationToken = default)
        => new Edit.DwfPackageStamper(_editor).StampAsync(inputDwfPath, outputDwfPath, stamps, cancellationToken);
}
