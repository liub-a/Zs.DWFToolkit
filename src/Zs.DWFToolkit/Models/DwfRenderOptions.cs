namespace Zs.DWFToolkit.Models;

public sealed class DwfRenderOptions
{
    /// <summary>Rasterization DPI used by external tools and simple PDF image sizing.</summary>
    public int Dpi { get; set; } = 200;

    /// <summary>Optional target width in pixels for tools that support it.</summary>
    public int? WidthPx { get; set; }

    /// <summary>Optional target height in pixels for tools that support it.</summary>
    public int? HeightPx { get; set; }

    /// <summary>png/jpg. DWFx/XPS conversion supports the formats accepted by mutool/gxps.</summary>
    public string ImageFormat { get; set; } = "png";

    public int JpegQuality { get; set; } = 90;
    public TimeSpan Timeout { get; set; } = TimeSpan.FromSeconds(60);
    public string? MutoolPath { get; set; }
    public string? GhostXpsPath { get; set; }

    /// <summary>Use the native DWF renderer first for ordinary DWF files when available.</summary>
    public bool PreferNativeDwfRenderer { get; set; } = false;

    /// <summary>
    /// Legacy diagnostic switch. Do not use embedded raster/page-preview resources as successful page rendering
    /// in production because those resources may be thumbnails, partial previews, or unrelated images.
    /// </summary>
    [Obsolete("Embedded raster resources are not reliable page renders. Use ExtractBestThumbnailAsync for thumbnails or a native W2D renderer for conversion.")]
    public bool ExtractRasterImagesFallback { get; set; } = false;

    /// <summary>Minimum embedded image size for the legacy diagnostic raster extractor.</summary>
    public long RasterImageMinBytes { get; set; } = 4096;

    /// <summary>When non-thumbnail raster resources exist, ignore thumbnail-named images in legacy diagnostic extraction.</summary>
    public bool SkipThumbnailWhenRasterImagesExist { get; set; } = true;

    /// <summary>
    /// Legacy diagnostic switch. Thumbnail extraction is exposed as ExtractBestThumbnailAsync and is not treated
    /// as a successful image conversion.
    /// </summary>
    [Obsolete("Thumbnail fallback is not drawing-grade conversion. Use ExtractBestThumbnailAsync explicitly.")]
    public bool ExtractThumbnailFallback { get; set; } = false;

    /// <summary>For ordinary DWF PDF conversion, render pages with the native renderer first and assemble them into PDF.</summary>
    public bool CreatePdfFromImagesFallback { get; set; } = true;

    /// <summary>Optional fixed page width in PDF points for the simple image PDF writer.</summary>
    public double? PdfPageWidthPoints { get; set; }

    /// <summary>Optional fixed page height in PDF points for the simple image PDF writer.</summary>
    public double? PdfPageHeightPoints { get; set; }

    /// <summary>Margin in PDF points used by the simple image PDF writer.</summary>
    public double PdfMarginPoints { get; set; } = 0;

    /// <summary>Keep temporary rendered images created during DWF to PDF fallback.</summary>
    public bool KeepTemporaryFiles { get; set; } = false;

    public bool Overwrite { get; set; } = true;
}
