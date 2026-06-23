using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Abstractions;

/// <summary>
/// Write-back editing of DWF/W2D graphics (e.g. stamping a signature/seal image).
/// Requires the native renderer build.
/// </summary>
public interface IDwfEditor
{
    /// <summary>Whether the native editing library is available.</summary>
    bool IsAvailable { get; }

    /// <summary>
    /// Stamps a raw RGBA image onto a W2D stream: copies the source W2D and appends
    /// the image at <paramref name="rect"/> (drawn on top), writing a new W2D file.
    /// <paramref name="rgbaPixels"/> is row-major <c>imageWidth*imageHeight*4</c> bytes.
    /// </summary>
    Task<DwfRenderResult> StampImageOnW2dAsync(
        string inputW2dPath,
        string outputW2dPath,
        byte[] rgbaPixels,
        int imageWidth,
        int imageHeight,
        DwfStampRect rect,
        CancellationToken cancellationToken = default);
}
