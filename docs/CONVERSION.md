# 转换能力说明

## 1. DWFx/XPS 转图片

优先使用 MuPDF `mutool`：

```bash
mutool draw -r 200 -o page-%03d.png input.dwfx
```

如果没有 `mutool`，托管层会尝试 GhostXPS/gxps：

```bash
gxps -sDEVICE=png16m -r200 -dNOPAUSE -dBATCH -o page-%03d.png input.dwfx
```

对应 C#：

```csharp
await toolkit.ConvertToImagesAsync("input.dwfx", "./out", new DwfRenderOptions
{
    Dpi = 200,
    ImageFormat = "png",
    MutoolPath = "/usr/local/bin/mutool"
});
```

## 2. DWFx/XPS 转 PDF

优先使用：

```bash
mutool convert -o output.pdf input.dwfx
```

备选 GhostXPS：

```bash
gxps -sDEVICE=pdfwrite -dNOPAUSE -dBATCH -o output.pdf input.dwfx
```

## 3. 普通 DWF 转图片

普通 `.dwf` 的 `to-images` 只接受真实页面渲染结果。

当前托管层策略：

```text
PreferNativeDwfRenderer = true 且 native renderer 可用
  → 调用 Native RenderPage
否则
  → 返回 unsupported_dwf_rendering
```

不再把以下内容当作转换成功：

- 包内 raster/page preview image；
- thumbnail；
- 任何无法证明是完整页面渲染结果的图片资源。

原因：这些资源常常只是缩略图、局部预览、缓存图或图标，用于签章定位会造成严重偏差。缩略图能力仍保留在 `ExtractBestThumbnailAsync` / `thumbnail` 命令中。

示例：

```csharp
var result = await toolkit.ConvertToImagesAsync("input.dwf", "./out", new DwfRenderOptions
{
    PreferNativeDwfRenderer = true,
    WidthPx = 4960,
    HeightPx = 3508,
    Dpi = 300
});
```

CLI：

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- \
  to-images input.dwf --out-dir ./out --native --dpi 300 --format jpg
```

## 4. 普通 DWF 转 PDF

普通 DWF 转 PDF 当前采用真实渲染后的“图片中间层”：

```text
Native 渲染每页图片
  → mutool convert 合成 PDF
  → 如果没有 mutool，内置 SimpleImagePdfWriter 尝试合成 PDF

Native 不可用
  → 返回 unsupported_dwf_rendering
```

不会再使用内嵌 raster preview 或 thumbnail 合成 PDF。

内置 `SimpleImagePdfWriter` 支持：

- JPEG 直接嵌入为 `/DCTDecode`；
- 非透明 RGB/灰度 PNG 以 `/FlateDecode + PNG Predictor` 嵌入；
- RGBA PNG、调色板 PNG、复杂 TIFF/WebP 需要外部工具，例如 `mutool`。

示例：

```csharp
var pdf = await toolkit.ConvertToPdfAsync("input.dwf", "preview.pdf", new DwfRenderOptions
{
    PreferNativeDwfRenderer = true,
    CreatePdfFromImagesFallback = true,
    ImageFormat = "jpg",
    Dpi = 200
});
```

CLI：

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- \
  to-pdf input.dwf --out preview.pdf --native --format jpg --dpi 200
```
