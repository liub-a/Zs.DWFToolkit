using System.Globalization;
using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;
using Zs.DWFToolkit.Abstractions;
using Zs.DWFToolkit.Models;

namespace Zs.DWFToolkit.Edit;

/// <summary>
/// DWF 包级写回：把印章贴进 DWF 内每页的 W2D 流并重新打包为合法 DWF。
///
/// 处理了 W2D 写回的全部包级细节：
/// 1. 坐标换算——W2D 头 (View x0,y0 x1,y1) 逻辑范围 ÷ (PlotInfo … mm W H) 真实毫米 = units/mm，
///    锚点在右下角、Y 轴自底向上。
/// 2. 多页——每个 .w2d 是一页，按 PageIndices 决定该页贴哪些章；一页多章用 native 链式逐个写回。
/// 3. 重打包——DWF 6 是 "(DWF V06.00)" 前缀 + ZIP。前缀使 ZIP 整体后移，必须把中央目录里的
///    本地头偏移与 EOCD 的中央目录偏移统一加上前缀长度，否则按绝对偏移读取的解析器找不到条目。
/// </summary>
public sealed class DwfPackageStamper(IDwfEditor editor)
{
    private static readonly Regex ViewRegex =
        new(@"\(View\s+(-?\d+),(-?\d+)\s+(-?\d+),(-?\d+)\)", RegexOptions.Compiled);
    private static readonly Regex PlotInfoRegex =
        new(@"\(PlotInfo\s+\S+\s+\S+\s+(?<unit>[A-Za-z]+)\s+(?<w>[0-9.]+)\s+(?<h>[0-9.]+)", RegexOptions.Compiled);

    /// <summary>对 DWF 落章并输出签署 DWF；返回实际落章数。</summary>
    public async Task<DwfRenderResult> StampAsync(
        string inputDwfPath, string outputDwfPath, IReadOnlyList<DwfImageStamp> stamps,
        CancellationToken cancellationToken = default)
    {
        if (!File.Exists(inputDwfPath))
        {
            return Fail(DwfErrorCodes.FileNotFound, "Input DWF not found.", inputDwfPath, outputDwfPath);
        }
        if (!editor.IsAvailable)
        {
            return Fail(DwfErrorCodes.NativeLibraryNotFound, "Native editor unavailable.", inputDwfPath, outputDwfPath);
        }

        var orig = await File.ReadAllBytesAsync(inputDwfPath, cancellationToken).ConfigureAwait(false);
        var prefix = ExtractDwfPrefix(orig);

        var w2dEntries = new List<string>();
        using (var probe = ZipFile.OpenRead(inputDwfPath))
        {
            foreach (var e in probe.Entries)
            {
                if (e.FullName.EndsWith(".w2d", StringComparison.OrdinalIgnoreCase))
                {
                    w2dEntries.Add(e.FullName);
                }
            }
        }
        if (w2dEntries.Count == 0)
        {
            return Fail(DwfErrorCodes.UnsupportedFormat, "No W2D page in DWF package.", inputDwfPath, outputDwfPath);
        }

        var tempDir = Path.Combine(Path.GetTempPath(), "zs-dwf-stamp", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);
        var stampedW2d = new Dictionary<string, byte[]>();
        var stampCount = 0;
        try
        {
            for (var pageIndex = 0; pageIndex < w2dEntries.Count; pageIndex++)
            {
                var entryName = w2dEntries[pageIndex];
                var pageStamps = stamps.Where(s => TargetsPage(s, pageIndex)).ToList();
                if (pageStamps.Count == 0)
                {
                    continue;
                }

                var current = Path.Combine(tempDir, $"p{pageIndex}_0.w2d");
                await ExtractEntryAsync(inputDwfPath, entryName, current, cancellationToken).ConfigureAwait(false);
                var geom = ReadGeometry(current);
                if (geom is null)
                {
                    continue;
                }

                var step = 0;
                foreach (var spec in pageStamps)
                {
                    if (spec.Rgba is null || spec.ImageWidth <= 0 || spec.ImageHeight <= 0)
                    {
                        continue;
                    }
                    var rect = RectFor(geom.Value, spec);
                    var next = Path.Combine(tempDir, $"p{pageIndex}_{++step}.w2d");
                    var r = await editor.StampImageOnW2dAsync(
                        current, next, spec.Rgba, spec.ImageWidth, spec.ImageHeight, rect, cancellationToken).ConfigureAwait(false);
                    if (!r.Success || !File.Exists(next))
                    {
                        return Fail(r.ErrorCode ?? DwfErrorCodes.NativeRenderFailed,
                            r.ErrorMessage ?? "W2D stamp failed.", inputDwfPath, outputDwfPath);
                    }
                    current = next;
                    stampCount++;
                }
                stampedW2d[entryName] = await File.ReadAllBytesAsync(current, cancellationToken).ConfigureAwait(false);
            }

            RepackageDwf(inputDwfPath, outputDwfPath, prefix, stampedW2d);
            return new DwfRenderResult
            {
                Success = true,
                ErrorCode = DwfErrorCodes.Ok,
                SourcePath = inputDwfPath,
                OutputPath = outputDwfPath,
                OutputFiles = { outputDwfPath },
                ToolName = "dwf-package-stamper",
            };
        }
        finally
        {
            try { Directory.Delete(tempDir, recursive: true); } catch { /* 临时目录清理失败忽略 */ }
        }
    }

    private static bool TargetsPage(DwfImageStamp s, int pageIndex) =>
        s.PageIndices is null || s.PageIndices.Count == 0 || s.PageIndices.Contains(pageIndex);

    private static byte[] ExtractDwfPrefix(byte[] dwf)
    {
        for (var i = 0; i < dwf.Length - 1; i++)
        {
            if (dwf[i] == (byte)'P' && dwf[i + 1] == (byte)'K')
            {
                return dwf[..i];
            }
        }
        return Array.Empty<byte>();
    }

    private static async Task ExtractEntryAsync(string dwfPath, string entryName, string outPath, CancellationToken ct)
    {
        using var zip = ZipFile.OpenRead(dwfPath);
        var entry = zip.GetEntry(entryName)
            ?? zip.Entries.First(e => string.Equals(e.FullName.Replace('\\', '/'), entryName.Replace('\\', '/'),
                StringComparison.OrdinalIgnoreCase));
        await using var es = entry.Open();
        await using var fs = File.Create(outPath);
        await es.CopyToAsync(fs, ct).ConfigureAwait(false);
    }

    private readonly record struct W2dGeometry(
        double ViewX0, double ViewY0, double ViewX1, double ViewY1, double PaperWmm, double PaperHmm);

    private static W2dGeometry? ReadGeometry(string w2dPath)
    {
        var bytes = File.ReadAllBytes(w2dPath);
        var head = Encoding.Latin1.GetString(bytes, 0, Math.Min(bytes.Length, 16 * 1024));

        var view = ViewRegex.Match(head);
        var plot = PlotInfoRegex.Match(head);
        if (!view.Success || !plot.Success)
        {
            return null;
        }
        double X(int g) => double.Parse(view.Groups[g].Value, CultureInfo.InvariantCulture);
        if (!double.TryParse(plot.Groups["w"].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var pw) ||
            !double.TryParse(plot.Groups["h"].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var ph))
        {
            return null;
        }
        var perMm = plot.Groups["unit"].Value.ToLowerInvariant() switch
        {
            "in" or "inch" or "inches" => 25.4,
            "cm" => 10.0,
            "m" => 1000.0,
            _ => 1.0,
        };
        return new W2dGeometry(X(1), X(2), X(3), X(4), pw * perMm, ph * perMm);
    }

    private static DwfStampRect RectFor(W2dGeometry g, DwfImageStamp s)
    {
        var unitsPerMmX = (g.ViewX1 - g.ViewX0) / Math.Max(1e-6, g.PaperWmm);
        var unitsPerMmY = (g.ViewY1 - g.ViewY0) / Math.Max(1e-6, g.PaperHmm);

        var rightMm = g.PaperWmm - s.OffsetXmm;
        var leftMm = rightMm - s.WidthMm;
        var bottomMm = s.OffsetYmm;
        var topMm = bottomMm + s.HeightMm;

        return new DwfStampRect(
            (int)Math.Round(g.ViewX0 + leftMm * unitsPerMmX),
            (int)Math.Round(g.ViewY0 + bottomMm * unitsPerMmY),
            (int)Math.Round(g.ViewX0 + rightMm * unitsPerMmX),
            (int)Math.Round(g.ViewY0 + topMm * unitsPerMmY));
    }

    private static void RepackageDwf(
        string srcDwfPath, string outputDwfPath, byte[] prefix, IReadOnlyDictionary<string, byte[]> stampedW2d)
    {
        var outDir = Path.GetDirectoryName(outputDwfPath);
        if (!string.IsNullOrEmpty(outDir))
        {
            Directory.CreateDirectory(outDir);
        }
        using var mem = new MemoryStream();
        using (var src = ZipFile.OpenRead(srcDwfPath))
        using (var zip = new ZipArchive(mem, ZipArchiveMode.Create, leaveOpen: true))
        {
            foreach (var e in src.Entries)
            {
                var ne = zip.CreateEntry(e.FullName, CompressionLevel.Optimal);
                using var os = ne.Open();
                if (stampedW2d.TryGetValue(e.FullName, out var replaced))
                {
                    os.Write(replaced, 0, replaced.Length);
                }
                else
                {
                    using var ies = e.Open();
                    ies.CopyTo(os);
                }
            }
        }

        var zipBytes = mem.ToArray();
        if (prefix.Length > 0)
        {
            PatchZipOffsets(zipBytes, (uint)prefix.Length);
        }
        using var ofs = File.Create(outputDwfPath);
        if (prefix.Length > 0)
        {
            ofs.Write(prefix, 0, prefix.Length);
        }
        ofs.Write(zipBytes, 0, zipBytes.Length);
    }

    // 中央目录条目"本地头偏移"与 EOCD"中央目录偏移"统一加上 delta（前缀字节数）。非 ZIP64 小包。
    private static void PatchZipOffsets(byte[] zip, uint delta)
    {
        var eocd = -1;
        for (var i = zip.Length - 22; i >= 0; i--)
        {
            if (zip[i] == 0x50 && zip[i + 1] == 0x4b && zip[i + 2] == 0x05 && zip[i + 3] == 0x06)
            {
                eocd = i;
                break;
            }
        }
        if (eocd < 0)
        {
            return;
        }

        int totalEntries = BitConverter.ToUInt16(zip, eocd + 10);
        var cdOffset = BitConverter.ToUInt32(zip, eocd + 16);
        var p = (int)cdOffset;
        for (var n = 0; n < totalEntries && p + 46 <= zip.Length; n++)
        {
            if (!(zip[p] == 0x50 && zip[p + 1] == 0x4b && zip[p + 2] == 0x01 && zip[p + 3] == 0x02))
            {
                break;
            }
            int nameLen = BitConverter.ToUInt16(zip, p + 28);
            int extraLen = BitConverter.ToUInt16(zip, p + 30);
            int commentLen = BitConverter.ToUInt16(zip, p + 32);
            AddUInt32(zip, p + 42, delta);
            p += 46 + nameLen + extraLen + commentLen;
        }
        AddUInt32(zip, eocd + 16, delta);
    }

    private static void AddUInt32(byte[] b, int pos, uint delta)
    {
        var v = BitConverter.ToUInt32(b, pos) + delta;
        BitConverter.GetBytes(v).CopyTo(b, pos);
    }

    private static DwfRenderResult Fail(string code, string msg, string src, string outp) => new()
    {
        Success = false,
        ErrorCode = code,
        ErrorMessage = msg,
        SourcePath = src,
        OutputPath = outp,
        ToolName = "dwf-package-stamper",
    };
}
