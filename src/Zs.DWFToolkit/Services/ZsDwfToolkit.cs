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
    private readonly IDwfPackageWriter _packageWriter;

    public ZsDwfToolkit(
        IDwfDocumentReader? reader = null,
        IDwfThumbnailExtractor? thumbnailExtractor = null,
        IDwfConverter? converter = null,
        IDwfPackageWriter? packageWriter = null)
    {
        _reader = reader ?? new DwfDocumentReader();
        _thumbnailExtractor = thumbnailExtractor ?? new DwfThumbnailExtractor();
        _converter = converter ?? new DwfxExternalConverter(_reader, _thumbnailExtractor, new NativeDwfRenderer());
        _packageWriter = packageWriter ?? new DwfPackageWriter();
    }

    public Task<DwfDocumentInfo> ReadInfoAsync(string sourcePath, CancellationToken cancellationToken = default)
        => _reader.ReadInfoAsync(sourcePath, cancellationToken);

    public Task<DwfThumbnailResult> ExtractBestThumbnailAsync(string sourcePath, string outputPath, CancellationToken cancellationToken = default)
        => _thumbnailExtractor.ExtractBestThumbnailAsync(sourcePath, outputPath, cancellationToken);

    public Task<DwfRenderResult> ConvertToPdfAsync(string sourcePath, string outputPdfPath, DwfRenderOptions? options = null, CancellationToken cancellationToken = default)
        => _converter.ConvertToPdfAsync(sourcePath, outputPdfPath, options, cancellationToken);

    public Task<DwfRenderResult> ConvertToImagesAsync(string sourcePath, string outputDirectory, DwfRenderOptions? options = null, CancellationToken cancellationToken = default)
        => _converter.ConvertToImagesAsync(sourcePath, outputDirectory, options, cancellationToken);

    public Task<DwfPackageWriteResult> ExtractAllAsync(string sourcePath, string outputDirectory, CancellationToken cancellationToken = default)
        => _packageWriter.ExtractAllAsync(sourcePath, outputDirectory, cancellationToken);

    public Task<DwfPackageWriteResult> CopyPackageAsync(string sourcePath, string destinationPath, CancellationToken cancellationToken = default)
        => _packageWriter.CopyPackageAsync(sourcePath, destinationPath, cancellationToken);

    public Task<DwfPackageWriteResult> ReplaceEntryAsync(string sourcePath, string destinationPath, string entryName, string replacementFilePath, CancellationToken cancellationToken = default)
        => _packageWriter.ReplaceEntryAsync(sourcePath, destinationPath, entryName, replacementFilePath, cancellationToken);
}
