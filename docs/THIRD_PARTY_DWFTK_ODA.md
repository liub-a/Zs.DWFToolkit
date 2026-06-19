# ODA 修改版 DWF Toolkit 集成说明

本仓库提供 ODA 修改版 Autodesk DWF Toolkit 7.7 的本地导入脚本。出于许可证和公开仓库分发风险，建议不要直接把第三方源码展开提交到公开仓库，而是在本地或私有仓库中导入到 `third_party/DWFToolkit-7.7`。


## 解压源码包

Linux/macOS：

```bash
./scripts/setup-oda-dwftoolkit.sh /path/to/DWFToolkit-7.7-src-ODA.zip
```

Windows PowerShell：

```powershell
./scripts/setup-oda-dwftoolkit.ps1 -Archive C:\path\DWFToolkit-7.7-src-ODA.zip
```

## 当前集成方式

Native 工程提供可选编译开关：

```bash
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda \
  -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda -j
```

默认不开启：

```bash
cmake -S src/Zs.DWFToolkit.Native -B build/native
cmake --build build/native -j
```

## 为什么默认关闭

上传的 `DWFToolkit-7.7-src-ODA.zip` 是 ODA 修改版 DWF Toolkit 源码，但不是完整 ODA SDK。它的 CMake 文件会引用 ODA KernelBase/平台层头文件，例如：

- `OdPlatformSettings.h`
- `OdInterlockedIncDec.h`

仓库里已在 `src/Zs.DWFToolkit.Native/compat/oda` 放入最小兼容头，用于尝试独立编译。但该源码仍可能在不同平台继续要求 ODA 平台层、编译选项或预编译二进制。因此它应被视为“可选第三方源码依赖”，而不是稳定的开箱即用渲染器。

## 目录说明

```text
third_party/DWFToolkit-7.7/
├── CMakeLists.txt
├── oda.cmake
├── th.cmake
├── th.components
├── develop/global/        # DwfCore / DwfToolkit / WhipTk / W3dTk 源码
└── docs/                  # 许可证与 Linux/Mac port 说明
```

## Native Bridge 对接点

当前 Native ABI 位于：

```text
src/Zs.DWFToolkit.Native/include/zs_dwf_toolkit_native.h
src/Zs.DWFToolkit.Native/src/zs_dwf_toolkit_native.cpp
```

需要逐步替换/增强：

```cpp
zs_dwf_get_info_json(...)
zs_dwf_render_page(...)
```

推荐后续新增：

```text
src/Zs.DWFToolkit.Native/src/oda/DwfToolkitPackageReader.cpp
src/Zs.DWFToolkit.Native/src/oda/WhipW2dRenderer.cpp
src/Zs.DWFToolkit.Native/src/render/SkiaRenderDevice.cpp
```

## 渲染实现路线

1. 用 DWF Toolkit 读取 package / section / resource；
2. 定位 ePlot section 的 W2D/WHIP resource；
3. 用 WhipTk 解析 W2D opcode；
4. 将线、折线、多边形、文字、图片等图元映射到 `IRenderDevice`；
5. 用 Skia/Cairo 输出 PNG/JPEG；
6. 多页图片再合成 PDF。

## 许可证提醒

DWF Toolkit 不是 MIT/BSD 许可证。使用、修改和分发时需要遵守 Autodesk DWF Toolkit License 及 ODA 修改包附带的说明。若仓库是公开仓库，建议再次确认是否允许公开分发该源码。
