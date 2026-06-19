#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/artifacts/native-build"
cmake -S "$ROOT/src/Zs.DWFToolkit.Native" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --config Release
mkdir -p "$ROOT/artifacts/native"
find "$BUILD" -name 'libzs_dwf_toolkit_native.so' -o -name 'zs_dwf_toolkit_native.dll' -o -name 'libzs_dwf_toolkit_native.dylib' | while read -r f; do cp "$f" "$ROOT/artifacts/native/"; done
echo "Native artifacts copied to $ROOT/artifacts/native"
