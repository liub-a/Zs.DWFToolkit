namespace Zs.DWFToolkit.Models;

/// <summary>
/// A 2D layer (WHIP WT_Layer) read from a DWF page's W2D stream.
/// </summary>
public sealed class DwfLayerInfo
{
    /// <summary>Layer number from the W2D stream (the WHIP layer id).</summary>
    public int Index { get; set; }

    /// <summary>Layer name (UTF-8 from the stream). May be empty if unnamed.</summary>
    public string Name { get; set; } = "";

    /// <summary>
    /// Initial visibility. The W2D format does not persist per-layer visibility, so the
    /// toolkit reports it as visible; layer show/hide is driven by the consumer at render
    /// time, not read from the file.
    /// </summary>
    public bool Visible { get; set; } = true;
}
