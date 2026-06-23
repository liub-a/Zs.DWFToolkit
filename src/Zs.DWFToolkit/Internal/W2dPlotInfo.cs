using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace Zs.DWFToolkit.Internal;

/// <summary>
/// Parses the real paper size from a W2D stream's leading ASCII header.
///
/// DWF ePlot W2D files begin with an ASCII opcode block that includes a
/// <c>(PlotInfo &lt;visibility&gt; &lt;flag&gt; &lt;units&gt; &lt;paperW&gt; &lt;paperH&gt; &lt;marginL&gt; &lt;marginB&gt; &lt;printableW&gt; &lt;printableH&gt; (matrix))</c>
/// record, e.g. <c>(PlotInfo show 0 mm 424 301 2 2 422 299 (...))</c>.
/// The paper dimensions are the true drawing sheet size (e.g. A3 ≈ 424×301 mm),
/// which the renderer otherwise loses by rasterizing to a fixed pixel canvas.
/// </summary>
internal static class W2dPlotInfo
{
    private const int HeaderProbeBytes = 16 * 1024;

    private static readonly Regex PlotInfoRegex = new(
        @"\(PlotInfo\s+\S+\s+\S+\s+(?<unit>[A-Za-z]+)\s+(?<w>[0-9]+(?:\.[0-9]+)?)\s+(?<h>[0-9]+(?:\.[0-9]+)?)",
        RegexOptions.Compiled);

    /// <summary>Reads the paper size (in millimetres) from a W2D stream, or null when not present.</summary>
    public static (double WidthMm, double HeightMm)? TryReadPaperMm(Stream w2dStream)
    {
        if (w2dStream is null) return null;

        var buffer = new byte[HeaderProbeBytes];
        var read = 0;
        int n;
        while (read < buffer.Length && (n = w2dStream.Read(buffer, read, buffer.Length - read)) > 0)
            read += n;
        if (read <= 0) return null;

        // The header is ASCII; Latin1 maps each byte 1:1 so trailing binary cannot corrupt the match.
        var head = Encoding.Latin1.GetString(buffer, 0, read);
        var match = PlotInfoRegex.Match(head);
        if (!match.Success) return null;

        if (!double.TryParse(match.Groups["w"].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var w) ||
            !double.TryParse(match.Groups["h"].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var h))
            return null;
        if (w <= 0 || h <= 0) return null;

        var perMm = MillimetresPerUnit(match.Groups["unit"].Value);
        return (w * perMm, h * perMm);
    }

    private static double MillimetresPerUnit(string unit) => unit.ToLowerInvariant() switch
    {
        "mm" => 1.0,
        "cm" => 10.0,
        "m" => 1000.0,
        "in" or "inch" or "inches" => 25.4,
        _ => 1.0, // DWF ePlot paper info is conventionally millimetres.
    };
}
