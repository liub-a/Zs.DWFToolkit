# Zs.DWFToolkit

`Zs.DWFToolkit` 是一个面向 .NET 的 DWF/DWFx 工具骨架，目标是给图纸签署系统提供 DWF 文件基础处理能力。

当前包含：

- DWF/DWFx/XPS 文件类型识别；
- ZIP/OPC 包结构读取；
- DWFx/XPS FixedPage 页信息读取；
- DWF 包内 `.w2d` 资源初步识别；
- 包内缩略图/预览图提取；
- DWFx/XPS 通过外部工具转 PDF；
- DWFx/XPS 通过外部工具转 PNG/JPEG；
- 普通 DWF 转图 Native Bridge 骨架；
- C# 调用 Demo；
- C++ Native ABI Stub；
- 设计文档和扩展说明。

## 重要边界

这个包 **没有内置完整 DWF/W2D 渲染器**。普通 `.dwf` 转 PNG/PDF 需要实现 Native 层的 DWF Toolkit + W2D/WHIP 渲染逻辑，或者接入商业渲染库。

当前能实际工作的部分：

| 能力 | 当前状态 |
|---|---|
| DWF/DWFx 包信息读取 | 已实现基础版 |
| DWF/DWFx 缩略图提取 | 已实现 |
| DWFx/XPS 转图片 | 已实现外部工具适配，依赖 `mutool` |
| DWFx/XPS 转 PDF | 已实现外部工具适配，依赖 `mutool` 或 `gxps` |
| 普通 DWF 转图片 | Native Bridge 已搭好，渲染器待实现 |
| 普通 DWF 转 PDF | 正式渲染待实现；可用缩略图 + mutool 做兜底 PDF |

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

### 6. 构建 Native Stub

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

## 后续实现普通 DWF 转图片的位置

核心位置：

```text
src/Zs.DWFToolkit.Native/src/zs_dwf_toolkit_native.cpp
```

需要替换 Stub：

```cpp
zs_dwf_get_info_json(...)
zs_dwf_render_page(...)
```

实现建议：

1. 接入 Autodesk/ODA DWF Toolkit；
2. 枚举 ePlot sections；
3. 找到每页 W2D/WHIP 图形流；
4. 实现 W2D 图元解释器；
5. 用 Skia/Cairo 渲染；
6. 输出 PNG/JPEG；
7. 返回页面尺寸、坐标变换和输出文件路径。

详见 `docs/NATIVE_INTEGRATION.md`。

## 推荐产品策略

第一版建议：

```text
普通 DWF：提取缩略图 + 要求上传配套 PDF
DWFx：尝试 mutool/gxps 转图片和 PDF
后续：如果客户强依赖普通 DWF 单文件预览，再实现 Native 渲染或接商业库
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
