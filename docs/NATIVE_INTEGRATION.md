# Native DWF Toolkit 集成说明

## 1. 当前状态

`src/Zs.DWFToolkit.Native` 是一个可编译的 C ABI Stub。它用于验证：

1. C# 能加载 Native DLL/SO；
2. P/Invoke 函数签名稳定；
3. Native 返回 JSON 给 C#；
4. 后续可以逐步替换为真实 DWF Toolkit 实现。

当前 Stub 不会真正渲染 DWF。

## 2. 为什么不用 C++ 类直接暴露给 C#

DWF Toolkit 是 C++ 库。C# 直接调用 C++ 类会遇到 ABI、对象生命周期、异常、编译器兼容问题。

所以本项目统一使用 C ABI：

```cpp
extern "C" int zs_dwf_get_info_json(...);
extern "C" int zs_dwf_render_page(...);
extern "C" const char* zs_dwf_get_last_error();
```

## 3. 推荐 Native 实现路线

```text
DWF Toolkit
  ↓
打开 DWF package
  ↓
枚举 sections
  ↓
定位 ePlot page
  ↓
读取 W2D/WHIP graphics resource
  ↓
W2D Interpreter
  ↓
Skia/Cairo RenderDevice
  ↓
PNG/JPEG Encoder
  ↓
JSON Result
```

## 4. 需要实现的 Native 功能

### 4.1 `zs_dwf_get_info_json`

输出示例：

```json
{
  "success": true,
  "errorCode": "ok",
  "sourcePath": "a.dwf",
  "kind": 1,
  "isZipBased": true,
  "pages": [
    {
      "pageIndex": 0,
      "pageName": "A0-建筑平面图",
      "width": 1189,
      "height": 841,
      "unit": "mm",
      "graphicsResourcePath": "...",
      "has2dGraphics": true
    }
  ],
  "entries": [],
  "properties": {}
}
```

### 4.2 `zs_dwf_render_page`

输入：

```cpp
const char* input_path;
int page_index;
const char* output_path;
int width_px;
int height_px;
int dpi;
```

输出：

```json
{
  "success": true,
  "errorCode": "ok",
  "sourcePath": "a.dwf",
  "pageIndex": 0,
  "outputPath": "page-001.png",
  "outputFiles": ["page-001.png"],
  "toolName": "dwf-toolkit-skia",
  "transform": {
    "pageWidth": 1189,
    "pageHeight": 841,
    "unit": "mm",
    "imageWidth": 4960,
    "imageHeight": 3508,
    "scaleX": 4.1715,
    "scaleY": 4.1712,
    "offsetX": 0,
    "offsetY": 0,
    "flipY": true
  }
}
```

## 5. W2D 渲染器建议拆分

```text
DwfPackageReader
DwfPageExtractor
W2dInterpreter
RenderState
IRenderDevice
SkiaRenderDevice / CairoRenderDevice
ImageEncoder
```

### IRenderDevice 示例

```cpp
class IRenderDevice
{
public:
    virtual void BeginPage(int widthPx, int heightPx) = 0;
    virtual void EndPage() = 0;
    virtual void SetStrokeColor(Color color) = 0;
    virtual void SetFillColor(Color color) = 0;
    virtual void SetLineWeight(double width) = 0;
    virtual void DrawLine(Point a, Point b) = 0;
    virtual void DrawPolyline(const std::vector<Point>& points) = 0;
    virtual void DrawPolygon(const std::vector<Point>& points) = 0;
    virtual void DrawText(const TextRun& text) = 0;
    virtual void DrawImage(const ImageData& image, Rect target) = 0;
    virtual bool Save(const std::string& outputPath) = 0;
};
```

## 6. 编译

```bash
./scripts/build-native.sh
```

输出在：

```text
artifacts/native/
```

运行 .NET Demo 前，把动态库复制到 Demo 输出目录，或者设置系统动态库搜索路径。

Linux：

```bash
export LD_LIBRARY_PATH=/path/to/native:$LD_LIBRARY_PATH
```

Windows：

把 `zs_dwf_toolkit_native.dll` 放到 exe 同目录。
