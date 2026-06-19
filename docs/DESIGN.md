# Zs.DWFToolkit 设计说明

## 1. 目标

为图纸签署系统提供 DWF/DWFx 基础处理能力：

1. 读取文件信息；
2. 提取缩略图；
3. DWFx 转图片/PDF；
4. 普通 DWF 预留 Native 渲染扩展；
5. 给前端预览和签章定位输出可用元数据。

## 2. 总体架构

```text
C# Web/API/Worker
    ↓
Zs.DWFToolkit 托管库
    ↓
DWFx: mutool/gxps 外部工具
DWF : Native Bridge / DWF Toolkit / W2D Renderer
    ↓
PNG/JPEG/PDF/Metadata
```

## 3. 模块

### 3.1 DwfDocumentReader

负责：

- 判断文件类型；
- 判断 ZIP/OPC 包；
- 读取包内 entries；
- 发现 DWFx/XPS `.fpage` 页面；
- 发现普通 DWF `.w2d` 资源；
- 发现缩略图资源。

### 3.2 DwfThumbnailExtractor

负责：

- 从包内寻找 `thumbnail` / `preview` / image resource；
- 输出到指定文件；
- 失败时返回明确错误码。

### 3.3 DwfxExternalConverter

负责：

- DWFx/XPS -> PDF；
- DWFx/XPS -> PNG/JPEG；
- 调用 `mutool` 或 `gxps`；
- 普通 DWF 不做假转换，只返回 `unsupported_dwf_rendering` 或缩略图兜底。

### 3.4 NativeDwfRenderer

负责：

- C# P/Invoke 调用 Native C ABI；
- 当前 Native 工程是 Stub；
- 后续接入 DWF Toolkit + W2D 渲染器。

## 4. 转换策略

```text
输入文件
  ↓
ReadInfo
  ↓
DWFx/XPS?
  ├─ 是：mutool/gxps 转 PDF/图片
  └─ 否：普通 DWF
        ├─ Native 渲染器可用：Native RenderPage
        ├─ Native 不可用：提取缩略图兜底
        └─ 失败：提示上传配套 PDF
```

## 5. 错误码

| 错误码 | 含义 |
|---|---|
| `file_not_found` | 文件不存在 |
| `invalid_package` | 包结构不合法 |
| `unsupported_format` | 不支持的文件类型 |
| `external_tool_not_found` | 找不到 mutool/gxps |
| `external_tool_failed` | 外部工具执行失败 |
| `native_library_not_found` | Native 动态库不存在 |
| `native_render_failed` | Native 渲染失败 |
| `unsupported_dwf_rendering` | 普通 DWF 渲染未实现 |
| `no_thumbnail` | 未找到缩略图 |
| `timeout` | 转换超时 |

## 6. 签章系统建议

生成预览结果后，签章坐标不要只保存图片像素坐标。建议保存：

```json
{
  "pageIndex": 0,
  "xRatio": 0.782,
  "yRatio": 0.843,
  "wRatio": 0.12,
  "hRatio": 0.06,
  "imageX": 3878,
  "imageY": 2957,
  "imageWidth": 595,
  "imageHeight": 210
}
```

最终归档仍建议用 PDF/OFD。
