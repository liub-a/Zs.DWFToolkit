using FluentAssertions;
using Xunit;
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Services;

namespace Zs.DWFToolkit.Tests.Edit;

public sealed class DwfPackageStamperTests
{
    private static string SampleDwf()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !File.Exists(Path.Combine(dir.FullName, "Zs.DWFToolkit.sln")))
        {
            dir = dir.Parent;
        }
        var root = dir?.FullName ?? ".";
        foreach (var rel in new[] { Path.Combine("tests", "Plot", "DWF", "2_A1.dwf"), Path.Combine("Plot", "DWF", "2_A1.dwf") })
        {
            var p = Path.Combine(root, rel);
            if (File.Exists(p)) { return p; }
        }
        return Path.Combine(root, "tests", "Plot", "DWF", "2_A1.dwf");
    }

    [Fact]
    public async Task StampImagesOnDwf_PreservesPaperSize_AndStaysValidDwf()
    {
        var src = SampleDwf();
        var toolkit = new ZsDwfToolkit();
        if (!File.Exists(src) || !toolkit.CanEdit)
        {
            return; // 无样本或无 native（如 CI 无 ODA）时跳过。
        }

        var rgba = new byte[200 * 80 * 4];
        for (var i = 0; i < 200 * 80; i++) { rgba[i * 4] = 220; rgba[i * 4 + 3] = 255; }
        var stamps = new[]
        {
            new DwfImageStamp(rgba, 200, 80, OffsetXmm: 60, OffsetYmm: 40, WidthMm: 40, HeightMm: 16),
        };

        var outDwf = Path.Combine(Path.GetTempPath(), $"zs-stamp-{Guid.NewGuid():N}.dwf");
        try
        {
            var r = await toolkit.StampImagesOnDwfAsync(src, outDwf, stamps);
            r.Success.Should().BeTrue($"{r.ErrorCode} {r.ErrorMessage}");
            File.Exists(outDwf).Should().BeTrue();

            // 签署 DWF 仍是合法 DWF，且真实图幅 mm 未丢（与原件一致）。
            var info = await toolkit.ReadInfoAsync(outDwf);
            var page = info.Pages.Should().ContainSingle().Subject;
            page.Unit.Should().Be("mm");
            page.Width.Should().NotBeNull();
            page.Width!.Value.Should().BeApproximately(845, 5);  // A1 + 边距
            page.Height!.Value.Should().BeApproximately(598, 5);
        }
        finally
        {
            if (File.Exists(outDwf)) { File.Delete(outDwf); }
        }
    }
}
