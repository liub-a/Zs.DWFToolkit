# 转换能力说明

## 1. DWFx/XPS 转图片

依赖 MuPDF `mutool`：

```bash
mutool draw -r 200 -o page-%03d.png input.dwfx
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
否则
  → 提取包内缩略图兜底
否则
  → 返回 unsupported_dwf_rendering
```

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

## 4. 普通 DWF 转 PDF

普通 DWF 转 PDF 当前不做完整渲染；已实现“提取缩略图 + mutool 包成 PDF”的兜底。正式路线：

1. Native 先渲染每页 PNG；
2. 再用 PDF 生成库把图片合成 PDF；
3. 或者接入真正的 CAD/DWF 渲染库；
4. 产品第一版要求上传配套 PDF。

## 5. 输出目录建议

```text
/storage/dwf-preview/{fileId}/
├── source/original.dwf
├── metadata/file-info.json
├── thumb/page-1.jpg
├── preview/page-001.png
└── pdf/preview.pdf
```
