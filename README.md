# Zs.DWFToolkit

`Zs.DWFToolkit` 是一个面向 .NET 的 DWF/DWFx 工具骨架，目标是给图纸签署系统提供 DWF 文件基础处理能力。

当前包含：

- DWF/DWFx/XPS 文件类型识别；
- ZIP/OPC 包结构读取；
- DWFx/XPS FixedPage 页信息读取；
- DWF 包内 `.w2d` 资源初步识别；
- 包内缩略图提取；
- DWFx/XPS 通过外部工具转 PDF；
- DWFx/XPS 通过 `mutool` 或 `gxps` 转 PNG/JPEG；
- 普通 DWF 通过 Native W2D 渲染器（ODA Toolkit + WHIP）转图片/PDF，真实工程图端到端验证；
- 没有 Native 渲染器时返回 `unsupported_dwf_rendering`，绝不把 raster/缩略图当成转换成功；
- 进程外渲染隔离（`zs_dwf_worker`）：native 崩溃/卡死不拖垮宿主；
- 自包含构建：FreeType/libtiff/zlib/字体源码入库，零第三方系统库；
- C# 调用 Demo + 多阶段 Dockerfile；
- 设计文档和扩展说明。

## 重要边界

普通 `.dwf` 转 PNG/PDF 走 Native 层的 ODA DWF Toolkit + W2D/WHIP 渲染器（`-DZS_DWF_ENABLE_ODA_DWFTK=ON` 构建）。
未构建 Native 渲染器时返回 `unsupported_dwf_rendering`，绝不拿缩略图冒充。3D（eModel）DWF 返回 `unsupported_3d_dwf`。

当前能实际工作的部分：

| 能力 | 当前状态 |
|---|---|
| DWF/DWFx 包信息读取 | 已实现（含 DWF v6 `(DWF Vxx)` 头部包） |
| DWF/DWFx 缩略图提取 | 已实现 |
| DWFx/XPS 转图片 / PDF | 外部工具适配，优先 `mutool`，兜底 `gxps` |
| 普通 DWF 转图片 / PDF | **Native W2D 渲染器已实现**（ODA 构建），真实工程图端到端验证 |
| Native 渲染器（W2D） | 折线/多边形/三角带/轮廓集/椭圆/真字形(FreeType)/标记/虚线/hatch/图层/裁剪/抗锯齿/图片(RGB·RGBA·mapped·JPEG·bitonal·G3/G4) |
| 进程外渲染隔离 | `ProcessNativeDwfRenderer` + `zs_dwf_worker`：崩溃→失败、卡死→超时，宿主存活 |
| 自包含构建 | FreeType/libtiff/zlib/字体 源码入库；jpeg 系统优先+6b 回退；零第三方系统库 |
| 容器 | 多阶段 `Dockerfile` |

## 工程结构

```text
Zs.DWFToolkit/
├── src/
│   ├── Zs.DWFToolkit/              # .NET 核心库
│   └── Zs.DWFToolkit.Native/       # C++ Native Bridge / Stub
├── samples/
│   └── Zs.DWFToolkit.CliDemo/      # 命令行调用示例
├── docs/                           # 设计文档
└── scripts/                        # 构建脚本
```

## 快速使用

### 1. 编译托管代码

```bash
dotnet build src/Zs.DWFToolkit/Zs.DWFToolkit.csproj
dotnet build samples/Zs.DWFToolkit.CliDemo/Zs.DWFToolkit.CliDemo.csproj
```

### 2. 查看文件信息

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info ./test.dwfx
```

### 3. 提取缩略图

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- thumbnail ./test.dwf --out ./out/thumb.jpg
```

### 4. DWFx 转图片

需要安装 MuPDF 的 `mutool`。

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./test.dwfx --out-dir ./out/images --dpi 200
```

也可以指定路径：

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./test.dwfx --out-dir ./out/images --mutool /usr/local/bin/mutool
```

### 5. DWFx 转 PDF

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-pdf ./test.dwfx --out ./out/test.pdf
```

### 6. 普通 DWF 转图片

普通 `.dwf` 不再默认提取包内 raster/page preview 或 thumbnail 作为页面转换结果，因为这些资源经常是不完整预览，不能用于签章定位。

必须接入真实 Native W2D/DWF 渲染器后再启用：

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./test.dwf --out-dir ./out/dwf-images --native --width 4960 --height 3508 --dpi 300
```

如果 Native 渲染器不可用，命令会返回 `unsupported_dwf_rendering`。

### 7. 普通 DWF 转 PDF

普通 `.dwf` 转 PDF 走真实渲染链：

```text
Native W2D/DWF 渲染每页图片
  → mutool convert 合成 PDF
  → 没有 mutool 时使用 SimpleImagePdfWriter 尝试合成栅格 PDF
```

没有 Native 渲染器时同样返回 `unsupported_dwf_rendering`，不会用缩略图冒充转换结果。

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-pdf ./test.dwf --out ./out/test.pdf --native --format jpg
```

### 8. 构建 Native Stub

```bash
./scripts/build-native.sh
```

Windows：

```powershell
./scripts/build-native.ps1
```

构建后把 `zs_dwf_toolkit_native.dll` / `libzs_dwf_toolkit_native.so` 放到 .NET 程序运行目录，C# 侧即可加载 Native Bridge。

## C# 调用示例

```csharp
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Services;

var toolkit = new ZsDwfToolkit();

var info = await toolkit.ReadInfoAsync("drawing.dwfx");
Console.WriteLine($"Kind={info.Kind}, Pages={info.PageCount}");

var thumb = await toolkit.ExtractBestThumbnailAsync("drawing.dwf", "thumb.jpg");

var images = await toolkit.ConvertToImagesAsync(
    "drawing.dwfx",
    "./preview",
    new DwfRenderOptions
    {
        Dpi = 200,
        ImageFormat = "png",
        Timeout = TimeSpan.FromSeconds(60)
    });

var pdf = await toolkit.ConvertToPdfAsync(
    "drawing.dwfx",
    "drawing.pdf");
```

## Native 渲染架构（已实现）

普通 DWF 渲染链（`-DZS_DWF_ENABLE_ODA_DWFTK=ON`）：

```text
DwfPackage(托管读 DWF v6 包) / OdaDwfAdapter(枚举 ePlot section -> 每页 W2D 流)
  → W2dWhipRenderer(WHIP opcode 回调)
  → RasterCanvas(矢量栅格化 + 2× 超采样抗锯齿)
  → MinimalPng(输出 PNG) + transform JSON
```

- 文字：FreeType 真字形，字体内嵌（`-DZS_DWF_WITH_FREETYPE=ON`）。
- Group4 传真图：vendored libtiff（`-DZS_DWF_WITH_TIFF=ON`）。
- 扩展图元：在 `src/w2d/W2dWhipRenderer.cpp` 加 `WT_*` 回调 + `RasterCanvas` 绘制原语，详见 `docs/W2D_RENDERING.md`。

## 进程外渲染隔离（产品化）

签署/批处理场景下，坏 DWF 或 native 故障不能拖垮宿主：

```csharp
var renderer = new ProcessNativeDwfRenderer();      // 找 zs_dwf_worker（同目录或 ZS_DWF_WORKER）
var converter = new DwfxExternalConverter(nativeRenderer: renderer);
var toolkit = new ZsDwfToolkit(converter: converter);
// worker 崩溃 → native_render_failed；卡死 → 超时杀进程；宿主进程始终存活
```

## 容器

```bash
docker build -t zs-dwftoolkit .
docker run --rm -v "$PWD:/data" zs-dwftoolkit to-images /data/drawing.dwf --out-dir /data/out --native
```

## ODA 修改版 DWF Toolkit

已集成 ODA 修改版 DWF Toolkit 的导入脚本和 Native CMake 开关。把你下载/上传的 `DWFToolkit-7.7-src-ODA.zip` 放到本地后，先解压到 `third_party/DWFToolkit-7.7` 作为可选 Native 依赖。默认构建仍使用轻量 Native Stub；需要尝试编译 ODA 修改版 Toolkit 时先执行：

```bash
./scripts/setup-oda-dwftoolkit.sh /path/to/DWFToolkit-7.7-src-ODA.zip
```

然后开启：

```bash
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda -j
```

详细说明见 `docs/THIRD_PARTY_DWFTK_ODA.md`。

## 许可证说明

本仓库代码默认不直接公开分发 Autodesk/ODA DWF Toolkit 源码或二进制；通过 `scripts/setup-oda-dwftoolkit.*` 在本地导入。你需要自行确认 DWF Toolkit 或第三方工具的授权。

## Native W2D rendering

The native layer now includes a first-pass W2D renderer built on the ODA-modified DWF Toolkit / WHIP Toolkit. Build it with:

```bash
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda --target zs_dwf_toolkit_native --config Release
```

It currently renders basic W2D primitives such as polylines, polygons and ellipses to PNG. Text is a placeholder and embedded images are not implemented yet. See `docs/W2D_RENDERING.md`.
