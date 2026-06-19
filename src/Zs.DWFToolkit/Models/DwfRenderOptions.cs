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

    /// <summary>Extract embedded raster page resources from ZIP/OPC style DWF packages when vector rendering is unavailable.</summary>
    public bool ExtractRasterImagesFallback { get; set; } = true;

    /// <summary>Minimum embedded image size to consider a page preview instead of an icon.</summary>
    public long RasterImageMinBytes { get; set; } = 4096;

    /// <summary>When non-thumbnail raster resources exist, ignore thumbnail-named images.</summary>
    public bool SkipThumbnailWhenRasterImagesExist { get; set; } = true;

    /// <summary>Extract the best embedded image when no page raster resources can be rendered/extracted.</summary>
    public bool ExtractThumbnailFallback { get; set; } = true;

    /// <summary>For ordinary DWF PDF conversion, render/extract images first and assemble them into PDF.</summary>
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
