#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/src/Zs.DWFToolkit.Native"
BUILD_DIR="$ROOT_DIR/artifacts/native-build"
OUT_DIR="$ROOT_DIR/artifacts/native"

mkdir -p "$BUILD_DIR" "$OUT_DIR"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release

find "$BUILD_DIR" -type f \( -name "*.so" -o -name "*.dylib" -o -name "*.dll" \) -exec cp {} "$OUT_DIR" \;
echo "Native build output copied to $OUT_DIR"
