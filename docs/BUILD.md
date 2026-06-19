# 构建说明

## 1. 托管库

需要 .NET 8 SDK：

```bash
dotnet build src/Zs.DWFToolkit/Zs.DWFToolkit.csproj -c Release
dotnet build samples/Zs.DWFToolkit.CliDemo/Zs.DWFToolkit.CliDemo.csproj -c Release
```

## 2. Native Stub

需要 CMake 和 C++ 编译器：

```bash
./scripts/build-native.sh
```

Windows：

```powershell
./scripts/build-native.ps1
```

## 3. 运行 Demo

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info input.dwfx
```

## 4. 目录部署

Windows：

```text
Zs.DWFToolkit.CliDemo.exe
Zs.DWFToolkit.dll
zs_dwf_toolkit_native.dll
```

Linux：

```text
Zs.DWFToolkit.CliDemo
Zs.DWFToolkit.dll
libzs_dwf_toolkit_native.so
```

Linux 可能需要：

```bash
export LD_LIBRARY_PATH=/app:$LD_LIBRARY_PATH
```
