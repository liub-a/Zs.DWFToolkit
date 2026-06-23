using System.Text;
using Xunit;
using Zs.DWFToolkit.Internal;

namespace Zs.DWFToolkit.Tests.Internal;

public class W2dPlotInfoTests
{
    private static Stream HeaderStream(string ascii) => new MemoryStream(Encoding.Latin1.GetBytes(ascii));

    [Fact]
    public void TryReadPaperMm_ParsesMillimetrePlotInfo()
    {
        var head = "(W2D V06.00)(Units '' ((1 0 0 0)))" +
                   "(PlotInfo show 0 mm 424 301 2 2 422 299 ((0.02 0 0)(0 0.02 0)(0 0 1)))";

        var result = W2dPlotInfo.TryReadPaperMm(HeaderStream(head));

        Assert.NotNull(result);
        Assert.Equal(424, result!.Value.WidthMm, 3);
        Assert.Equal(301, result.Value.HeightMm, 3);
    }

    [Fact]
    public void TryReadPaperMm_ConvertsInchesToMillimetres()
    {
        var head = "(PlotInfo show 0 in 11 8.5 0 0 11 8.5 (()))";

        var result = W2dPlotInfo.TryReadPaperMm(HeaderStream(head));

        Assert.NotNull(result);
        Assert.Equal(279.4, result!.Value.WidthMm, 2); // 11 in
        Assert.Equal(215.9, result.Value.HeightMm, 2); // 8.5 in
    }

    [Fact]
    public void TryReadPaperMm_ReturnsNullWhenPlotInfoMissing()
    {
        var head = "(W2D V06.00)(Units '' ((1 0 0 0)))(View 0,0 100,100)";

        Assert.Null(W2dPlotInfo.TryReadPaperMm(HeaderStream(head)));
    }
}
