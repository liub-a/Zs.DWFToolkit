namespace Zs.DWFToolkit.Models;

public sealed class DwfRenderOptions
{
    public int Dpi { get; set; } = 200;
    public int? WidthPx { get; set; }
    public int? HeightPx { get; set; }
    public string ImageFormat { get; set; } = "png";
    public int JpegQuality { get; set; } = 90;
    public TimeSpan Timeout { get; set; } = TimeSpan.FromSeconds(60);
    public string? MutoolPath { get; set; }
    public string? GhostXpsPath { get; set; }
    public bool PreferNativeDwfRenderer { get; set; } = false;
    public bool ExtractThumbnailFallback { get; set; } = true;
    public bool Overwrite { get; set; } = true;
}
