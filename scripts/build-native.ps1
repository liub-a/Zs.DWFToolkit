$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$src = Join-Path $root "src/Zs.DWFToolkit.Native"
$build = Join-Path $root "artifacts/native-build"
$out = Join-Path $root "artifacts/native"

New-Item -ItemType Directory -Force -Path $build | Out-Null
New-Item -ItemType Directory -Force -Path $out | Out-Null

cmake -S $src -B $build -DCMAKE_BUILD_TYPE=Release
cmake --build $build --config Release

Get-ChildItem -Path $build -Recurse -Include "*.dll","*.so","*.dylib" | ForEach-Object {
    Copy-Item $_.FullName -Destination $out -Force
}

Write-Host "Native build output copied to $out"
