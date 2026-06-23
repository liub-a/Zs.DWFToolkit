# 三方接入说明（Integration Guide）

把 `Zs.DWFToolkit` 嵌入你的应用（签署系统、预览服务、批处理 worker）时的接入与部署指南。
API 方法细节见 [API.md](API.md)；渲染能力边界见 [W2D_RENDERING.md](W2D_RENDERING.md)。

## 1. 能力一览

| 输入 | 操作 | 依赖 |
|---|---|---|
| DWF / DWFx / XPS | 包信息读取、缩略图提取 | 纯托管，无外部依赖 |
| DWFx / XPS | 转图片 / PDF | 外部 `mutool`（优先）或 `gxps` |
| 普通 DWF（含 v6 `(DWF Vxx)` 头） | 转图片 / PDF | **Native 渲染器**（ODA 构建） |

普通 DWF 不会拿缩略图冒充转换：无 Native 渲染器时返回 `unsupported_dwf_rendering`，3D（eModel）返回 `unsupported_3d_dwf`。

## 2. 引用托管库

### NuGet 包（推荐）

打包（先按 §3 构建好 native 到 `artifacts/native/<rid>/`，再 pack）：

```bash
dotnet pack src/Zs.DWFToolkit/Zs.DWFToolkit.csproj -c Release -o artifacts/nuget
```

包内含托管 `lib/net8.0/` + 各平台 native `runtimes/<rid>/native/`（`libzs_dwf_toolkit_native.*`
+ `zs_dwf_worker`）。消费方引用即自动按运行平台解析 native：

```bash
dotnet add package Zs.DWFToolkit --source path/to/artifacts/nuget
```

P/Invoke 与进程外 worker 都会从 `runtimes/<rid>/native/` 自动定位（`ProcessNativeDwfRenderer`
也会探测该路径）。打包前需把目标平台的 native 产物（**库 + `zs_dwf_worker`**）放进
`artifacts/native/<rid>/`；未构建的平台会被跳过。

CI 的 `native-oda` 作业已对 **`linux-x64` 和 `linux-arm64`** 构建并上传完整产物
（库 + worker）为 `native-oda-<rid>` artifact；下载解压到 `artifacts/native/<rid>/`
再 `dotnet pack` 即可。`osx-arm64` / `osx-x64` / `win-x64` 需在对应平台各自构建后放入。
注意：每个 RID 都要带 `zs_dwf_worker`，否则该平台只能进程内渲染（无崩溃/超时隔离）。

### 项目引用（源码集成）

```xml
<ProjectReference Include="path/to/src/Zs.DWFToolkit/Zs.DWFToolkit.csproj" />
```

目标框架 `net8.0`。最简用法：

```csharp
using Zs.DWFToolkit.Models;
using Zs.DWFToolkit.Services;

var toolkit = new ZsDwfToolkit();
var info  = await toolkit.ReadInfoAsync("drawing.dwf");
var imgs  = await toolkit.ConvertToImagesAsync("drawing.dwf", "./out",
              new DwfRenderOptions { PreferNativeDwfRenderer = true });
```

## 3. 部署 Native 渲染器（普通 DWF 必需）

普通 DWF 渲染需要 Native 库；DWFx/XPS 不需要。

1. 构建（首次先 `./scripts/setup-oda-dwftoolkit.sh` 导入 ODA 源）：

   ```bash
   cmake -S src/Zs.DWFToolkit.Native -B build/native \
         -DZS_DWF_ENABLE_ODA_DWFTK=ON -DZS_DWF_WITH_FREETYPE=ON -DZS_DWF_WITH_TIFF=ON
   cmake --build build/native -j 2
   ```

   第三方库（FreeType / libtiff / zlib / 字体）已全部 vendored，构建只需 `cmake` + `g++`，无系统库、无联网。

2. 把产物放到托管程序运行目录：

   - `libzs_dwf_toolkit_native.{so,dylib}` / `zs_dwf_toolkit_native.dll`（P/Invoke 加载）
   - `zs_dwf_worker`（进程外渲染，见 §5）

   Linux 上设 `LD_LIBRARY_PATH` 或放在可执行同目录。

## 4. 渲染模式：进程内 vs 进程外

| 模式 | 类 | 适用 |
|---|---|---|
| 进程内（默认） | `NativeDwfRenderer` | 受信任输入、单文件工具 |
| **进程外（推荐生产）** | `ProcessNativeDwfRenderer` | 批量/不可信 DWF、服务端 |

进程外把渲染放到 `zs_dwf_worker` 子进程，**崩溃/卡死不拖垮宿主**：

```csharp
var renderer  = new ProcessNativeDwfRenderer();   // 找 zs_dwf_worker（同目录或 ZS_DWF_WORKER 环境变量）
var converter = new DwfxExternalConverter(nativeRenderer: renderer);
var toolkit   = new ZsDwfToolkit(converter: converter);

var r = await toolkit.ConvertToImagesAsync("untrusted.dwf", "./out",
          new DwfRenderOptions { PreferNativeDwfRenderer = true, Timeout = TimeSpan.FromSeconds(30) });
// worker 崩溃 → r.ErrorCode == native_render_failed；卡死 → 杀进程 → timeout；宿主存活
```

## 5. 外部工具（DWFx / XPS）

DWFx/XPS 转图片/PDF 需要其一在 `PATH` 或在 `DwfRenderOptions` 指定：

- MuPDF `mutool`（优先）：`MutoolPath`
- GhostXPS `gxps`（兜底）：`GhostXpsPath`

无外部工具时返回 `external_tool_not_found`。

## 6. 关键选项（`DwfRenderOptions`）

| 字段 | 说明 |
|---|---|
| `PreferNativeDwfRenderer` | 普通 DWF 用 Native 渲染（否则返回 unsupported）|
| `Dpi` / `WidthPx` / `HeightPx` | 输出尺寸 |
| `ImageFormat` | `png` / `jpg`（DWFx 经外部工具）|
| `Timeout` | 单次渲染超时（进程外模式下用于杀子进程）|
| `MutoolPath` / `GhostXpsPath` | 外部工具路径 |
| `CreatePdfFromImagesFallback` | 普通 DWF→PDF：Native 渲图 → 合成 PDF |

## 7. 错误码（消费侧）

返回对象的 `ErrorCode` 取自 `Models/DwfErrorCodes`：

| 码 | 含义 / 处理 |
|---|---|
| `ok` | 成功 |
| `file_not_found` / `invalid_argument` | 输入问题 |
| `unsupported_format` | 非 DWF/DWFx/XPS 包 |
| `unsupported_dwf_rendering` | 普通 DWF 但无 Native 渲染器 → 部署 native |
| `unsupported_3d_dwf` | 3D（eModel）DWF，本库不渲染 3D |
| `external_tool_not_found` | DWFx 缺 mutool/gxps |
| `native_library_not_found` | 找不到 native 库/worker |
| `native_render_failed` | Native 渲染失败（含 worker 崩溃）|
| `timeout` | 超时（含 worker 被杀）|

约定：始终用 `DwfErrorCodes` 常量比较，勿硬编码字符串。

## 8. 容器接入

```bash
docker build -t zs-dwftoolkit .
docker run --rm -v "$PWD:/data" zs-dwftoolkit \
  to-images /data/drawing.dwf --out-dir /data/out --native
```

镜像内含 native 库 + worker + mutool，`LD_LIBRARY_PATH`/`ZS_DWF_WORKER` 预设。

## 9. 边界（接入前须知）

- **3D（eModel）DWF**：不渲染，返回 `unsupported_3d_dwf`。
- **SHX 矢量字体**：用内嵌 DejaVu 渲染，不逐字形还原 Autodesk 专有 SHX。
- **宏（macro）**：放置点标记，未完整重放宏几何。
- 输出为 **预览/编辑级**，非归档级高保真。

签署/盖章定位：用 `DwfRenderResult.Transform`（`DwfPageTransform`）把图纸坐标映射到像素。
