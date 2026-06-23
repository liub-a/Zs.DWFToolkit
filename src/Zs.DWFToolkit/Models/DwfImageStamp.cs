namespace Zs.DWFToolkit.Models;

/// <summary>
/// 一个要贴到 DWF 上的印章：原始 RGBA 像素 + 以右下角为锚点的毫米位置/尺寸 + 目标页。
/// 坐标由 W2D 的 (View)÷(PlotInfo) 自动换算到逻辑单位，Y 轴自底向上。
/// </summary>
public sealed record DwfImageStamp(
    byte[] Rgba,
    int ImageWidth,
    int ImageHeight,
    double OffsetXmm,
    double OffsetYmm,
    double WidthMm,
    double HeightMm,
    IReadOnlyList<int>? PageIndices = null);
