namespace Zs.DWFToolkit.Models;

/// <summary>
/// Target rectangle for a stamped image, in W2D drawing (logical) coordinates.
/// Use <see cref="DwfPageTransform"/> from a render result to convert pixel or
/// normalized positions into these logical units.
/// </summary>
public readonly record struct DwfStampRect(int MinX, int MinY, int MaxX, int MaxY);
