namespace Zs.DWFToolkit.Models;

public sealed class DwfPageInfo
{
    public int PageIndex { get; set; }
    public string? PageName { get; set; }
    public double? Width { get; set; }
    public double? Height { get; set; }
    public string Unit { get; set; } = "unknown";
    public string? GraphicsResourcePath { get; set; }
    public string? ThumbnailResourcePath { get; set; }
    public bool Has2dGraphics { get; set; }
    public bool Has3dGraphics { get; set; }
    public bool IsXpsFixedPage { get; set; }

    /// <summary>
    /// 2D layers (WHIP WT_Layer) on this page, in order of appearance. Empty when the
    /// drawing has no layers (e.g. AutoCAD plate plots) or when layers were not parsed
    /// (see <see cref="LayersParsed"/>). Populated by the native renderer.
    /// </summary>
    public IReadOnlyList<DwfLayerInfo> Layers { get; set; } = [];

    /// <summary>
    /// Whether layer information was parsed for this page. Distinguishes "no layers"
    /// (parsed, empty list) from "not parsed" (format unsupported or reading skipped).
    /// </summary>
    public bool LayersParsed { get; set; }
}
