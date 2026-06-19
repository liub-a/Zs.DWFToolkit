namespace Zs.DWFToolkit.Models;

public sealed class DwfPageTransform
{
    public double? PageWidth { get; set; }
    public double? PageHeight { get; set; }
    public string Unit { get; set; } = "unknown";
    public int ImageWidth { get; set; }
    public int ImageHeight { get; set; }
    public double? ScaleX { get; set; }
    public double? ScaleY { get; set; }
    public double OffsetX { get; set; }
    public double OffsetY { get; set; }
    public bool FlipY { get; set; } = true;
}
