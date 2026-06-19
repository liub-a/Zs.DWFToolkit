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

当前托管层策略：

```text
PreferNativeDwfRenderer = true 且 native renderer 可用
  → 调用 Native RenderPage
否则，如果 DWF 包内有 raster page/preview image
  → 提取内嵌 raster 图片作为页面预览
否则，如果有 thumbnail
  → 提取 thumbnail 兜底
否则
  → 返回 unsupported_dwf_rendering
```

示例：

```csharp
var result = await toolkit.ConvertToImagesAsync("input.dwf", "./out", new DwfRenderOptions
{
    PreferNativeDwfRenderer = true,
    ExtractRasterImagesFallback = true,
    ExtractThumbnailFallback = true,
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

普通 DWF 转 PDF 当前采用“图片中间层”：

```text
Native 渲染每页图片
  → mutool convert 合成 PDF
  → 如果没有 mutool，内置 SimpleImagePdfWriter 尝试合成 PDF

Native 不可用
  → 提取内嵌 raster page/preview images
  → 合成 PDF

仍不可用
  → 提取 thumbnail
  → 合成单页 fallback PDF
```

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

## 5. 关键选项

| 选项 | 说明 |
|---|---|
| `PreferNativeDwfRenderer` | 普通 DWF 优先走 Native Bridge |
| `ExtractRasterImagesFallback` | Native 不可用时提取包内 raster 图片 |
| `RasterImageMinBytes` | 过滤过小 icon/资源，默认 4096 |
| `SkipThumbnailWhenRasterImagesExist` | 有页面 raster 时跳过 thumbnail |
| `ExtractThumbnailFallback` | 最后兜底提取 thumbnail |
| `CreatePdfFromImagesFallback` | 普通 DWF PDF 走图片合成 |
| `PdfPageWidthPoints/PdfPageHeightPoints` | 内置 PDF writer 固定页尺寸 |
| `PdfMarginPoints` | 内置 PDF writer 页边距 |

## 6. 输出目录建议

```text
/storage/dwf-preview/{fileId}/
├── source/original.dwf
├── metadata/file-info.json
├── thumb/page-1.jpg
├── preview/page-001.png
└── pdf/preview.pdf
```

## 7. 重要边界

普通 DWF 的高保真矢量转图仍然需要完整 W2D/WHIP 渲染器。当前版本已经把可工作的转换链和兜底链补齐，但 raster fallback 不是 CAD 级渲染。
