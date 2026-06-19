$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot/.."
$Build = Join-Path $Root "artifacts/native-build"
cmake -S (Join-Path $Root "src/Zs.DWFToolkit.Native") -B $Build -DCMAKE_BUILD_TYPE=Release
cmake --build $Build --config Release
New-Item -ItemType Directory -Force -Path (Join-Path $Root "artifacts/native") | Out-Null
Get-ChildItem -Path $Build -Recurse -Include "zs_dwf_toolkit_native.dll","libzs_dwf_toolkit_native.so","libzs_dwf_toolkit_native.dylib" | Copy-Item -Destination (Join-Path $Root "artifacts/native") -Force
Write-Host "Native artifacts copied to $Root/artifacts/native"
