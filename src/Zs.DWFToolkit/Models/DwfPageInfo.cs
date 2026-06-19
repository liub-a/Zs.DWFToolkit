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
}
