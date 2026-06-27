#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_ARCHIVE="$ROOT_DIR/third_party/archives/DWFToolkit-7.7-src-ODA-essential.tar.gz"
INPUT_ARCHIVE="${1:-$DEFAULT_ARCHIVE}"
TARGET="$ROOT_DIR/third_party/DWFToolkit-7.7"
TMP_DIR="$ROOT_DIR/third_party/.tmp-dwftk-import"

if [[ ! -f "$INPUT_ARCHIVE" ]]; then
  echo "Archive not found: $INPUT_ARCHIVE" >&2
  echo "Usage: $0 /path/to/DWFToolkit-7.7-src-ODA.zip" >&2
  echo "   or: $0 /path/to/DWFToolkit-7.7-src-ODA-essential.tar.gz" >&2
  exit 1
fi

rm -rf "$TARGET" "$TMP_DIR"
mkdir -p "$ROOT_DIR/third_party"

case "$INPUT_ARCHIVE" in
  *.tar.gz|*.tgz)
    tar -xzf "$INPUT_ARCHIVE" -C "$ROOT_DIR/third_party"
    ;;
  *.zip)
    mkdir -p "$TMP_DIR"
    unzip -q "$INPUT_ARCHIVE" -d "$TMP_DIR"
    if [[ ! -d "$TMP_DIR/DWFToolkit-7.7" ]]; then
      echo "Expected DWFToolkit-7.7 folder inside zip." >&2
      exit 2
    fi
    mv "$TMP_DIR/DWFToolkit-7.7" "$TARGET"
    rm -rf "$TMP_DIR"
    ;;
  *)
    echo "Unsupported archive type: $INPUT_ARCHIVE" >&2
    exit 3
    ;;
esac

# WebAssembly (Emscripten) support: DWFCore's platform detection in Core.h has no
# branch for wasm and hits "#error DWFCORE Undefined Platform". Add one (mapping
# wasm to the ARM system, a known-good little-endian/no-asm config). Idempotent.
CORE_H="$TARGET/develop/global/src/dwfcore/Core.h"
if [[ -f "$CORE_H" ]] && ! grep -q "__wasm__" "$CORE_H"; then
  perl -0pi -e 's/(#elif\s+defined\(__arm__\) \|\| defined\(__aarch64__\)\n#define _DWFCORE_ARM_SYSTEM\n)(#else\n#error DWFCORE Undefined Platform)/${1}#elif   defined(__wasm__) || defined(__wasm32__) || defined(__EMSCRIPTEN__)\n#define _DWFCORE_ARM_SYSTEM\n${2}/' "$CORE_H"
  if grep -q "__wasm__" "$CORE_H"; then
    echo "Applied WebAssembly platform patch to dwfcore/Core.h"
  else
    echo "WARNING: failed to apply WebAssembly platform patch to dwfcore/Core.h" >&2
  fi
fi

echo "ODA-modified DWFToolkit extracted to: $TARGET"
