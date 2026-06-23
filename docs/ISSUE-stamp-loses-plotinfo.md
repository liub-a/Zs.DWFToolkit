# BUG：StampImageOnW2dAsync 写回后丢失 (View)/(PlotInfo) 等图幅元数据

## 现象

调用 `StampImageOnW2dAsync` 把印章写回 W2D 后，输出的 W2D 渲染时**纵横比错误**：
退回到默认画布 2400×1600，而不是按真实图幅（A1 845×598mm）的比例渲染。

下游依据 `ReadInfoAsync` 的 `Page.Width/Height`（解析 W2D 头部 `(PlotInfo … mm W H)`）做图幅匹配
与按真实 mm 渲染；写回后这些信息消失，导致：

1. 签署后的 DWF 预览/打印尺寸不对（图被拉伸变形）。
2. `ReadInfoAsync` 返回 `Page.Width/Height = null`、`Unit != "mm"`。

## 根因（已定位）

写回会把 W2D **整体重新序列化**，且**不保留原 W2D 头部的关键记录**。

原始 W2D（`Plot/DWF/2_A1.dwf` 内）头部：

```
(W2D V06.00)
(Units '' ((47.24… 0 0 0) …))
(View 0,0 39732,28062)
(PlotInfo show 0 mm 845 598 2 2 843 596 (…))   ← 图幅真实毫米尺寸
```

写回后（signed DWF 内的 W2D）头部：

```
(W2D V06.01)            ← 版本被改写
(Units '' (…))          ← 保留
                        ← (View …) 丢失
                        ← (PlotInfo …) 丢失
```

对比数据：
- 原 W2D：约 36 KB，含 `(View)`、`(PlotInfo)`。
- 写回 W2D：约 3.5 MB（未压缩），`(View)`/`(PlotInfo)` 在前 16KB 内均不存在。

## 复现

```csharp
using System.IO.Compression;
var src = "Plot/DWF/2_A1.dwf";
// 1) 抽 .w2d
string w2d = "page.w2d", entry;
using (var z = ZipFile.OpenRead(src)) {
    var e = z.Entries.First(x => x.FullName.EndsWith(".w2d", StringComparison.OrdinalIgnoreCase));
    entry = e.FullName; e.ExtractToFile(w2d, true);
}
// 2) 写回一张图（坐标按 View÷PlotInfo 换算，本 bug 与坐标无关）
var rgba = new byte[600 * 266 * 4];
var r = await new ZsDwfToolkit().StampImageOnW2dAsync(
    w2d, "out.w2d", rgba, 600, 266, new DwfStampRect(34000, 1200, 37000, 1900));
// 3) 检查 out.w2d 头部：(View)/(PlotInfo) 已不在；版本变 V06.01；体积暴涨。
var head = System.Text.Encoding.Latin1.GetString(File.ReadAllBytes("out.w2d")[..16384]);
Console.WriteLine(head.Contains("(PlotInfo")); // False（期望 True）
Console.WriteLine(head.Contains("(View "));    // False（期望 True）
```

## 期望

写回后输出的 W2D 应：

1. **保留原 W2D 头部记录**，至少 `(View)`、`(PlotInfo)`、`(Units)`（图幅尺寸/视图/单位不可丢），
   使 `ReadInfoAsync` 仍能解析出真实 mm、渲染保持正确纵横比。
2. 尽量**最小改动**写回（在原流上追加印章对象），避免整体重序列化导致体积暴涨
   （36KB → 3.5MB）。若必须重写，也应原样回写上述元数据记录。
3. 可选：保持 `(W2D V06.00)` 版本号或确保 V06.01 被渲染器/ReadInfo 同等支持。

## 影响

- 签署系统：DWF 原生写回签署后，签名能正确贴入（功能 OK），但签署 DWF 预览/打印尺寸变形。
- 当前规避：渲染签署 DWF 时由调用方显式传入已知 width/height（来自原件 PlotInfo），
  但**外部 DWF 阅读器**仍读不到图幅，需 SDK 层修复。

## 环境

- Zs.DWFToolkit 0.5.1（osx-arm64 / linux-x64，均含 `zs_w2d_stamp_image` 导出）。
- 样本：`Plot/DWF/2_A1.dwf`（A1，`(DWF V06.00)`，单页 W2D）。
