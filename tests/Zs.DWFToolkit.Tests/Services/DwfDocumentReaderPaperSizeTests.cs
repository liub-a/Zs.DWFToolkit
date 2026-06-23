using System.Text;
using Xunit;
using Zs.DWFToolkit.Services;

namespace Zs.DWFToolkit.Tests.Services;

public class DwfDocumentReaderPaperSizeTests
{
    [Fact]
    public async Task ReadInfoAsync_FillsPaperMillimetres_FromW2dPlotInfo()
    {
        var w2dHeader = "(W2D V06.00)(Units '' ((1 0 0 0)))" +
                        "(PlotInfo show 0 mm 424 301 2 2 422 299 ((0.02 0 0)(0 0.02 0)(0 0 1)))";
        var package = TestData.WritePackage(".dwf", new Dictionary<string, byte[]>
        {
            ["com.autodesk.dwf.ePlot_X/page.w2d"] = Encoding.Latin1.GetBytes(w2dHeader),
        });

        try
        {
            var info = await new DwfDocumentReader().ReadInfoAsync(package);

            var page = Assert.Single(info.Pages);
            Assert.Equal("mm", page.Unit);
            Assert.NotNull(page.Width);
            Assert.NotNull(page.Height);
            Assert.Equal(424, page.Width!.Value, 3);
            Assert.Equal(301, page.Height!.Value, 3);
        }
        finally
        {
            File.Delete(package);
        }
    }
}
