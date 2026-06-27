#!/usr/bin/env bash
# Builds the native DWF stack (ODA DwfCore/WhipTk/W3dTk/DwfToolkit + FreeType + our
# renderer) to a single WebAssembly module exporting the C ABI, for a pure
# front-end, offline, in-browser DWF renderer.
#
# Requires Emscripten (emsdk or a working `emcc`/`emcmake` on PATH). On systems
# whose default `python3` is < 3.10 (emscripten needs >=3.10), set EMSDK_PYTHON to a
# newer interpreter. If a misconfigured install points LLVM_ROOT at a missing dir,
# set EM_CONFIG to a corrected config file.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/native-wasm}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/wasm/dwf-web}"
JOBS="${JOBS:-2}"

command -v emcc >/dev/null || { echo "emcc not found on PATH (install emsdk)" >&2; exit 1; }

# Import the ODA toolkit source (idempotently applies the wasm platform patch).
if [[ ! -f "$ROOT_DIR/third_party/DWFToolkit-7.7/CMakeLists.txt" ]]; then
  "$ROOT_DIR/scripts/setup-oda-dwftoolkit.sh"
fi

# Configure (Emscripten branch in CMakeLists enables -DEMCC, -Wno-register, etc.).
# TIFF is off (its Group4 fax codec is optional and needs a cross-compile nudge).
emcmake cmake -S "$ROOT_DIR/src/Zs.DWFToolkit.Native" -B "$BUILD_DIR" \
  -DZS_DWF_ENABLE_ODA_DWFTK=ON -DZS_DWF_WITH_FREETYPE=ON -DZS_DWF_WITH_TIFF=OFF \
  -DZS_DWF_BUILD_WORKER=OFF -DCMAKE_BUILD_TYPE=Release

# Build the static archives (-j2: each WhipTk TU needs ~GBs; unbounded OOM-kills).
emmake cmake --build "$BUILD_DIR" -j"$JOBS"

# Final link: combine the archives into one JS+wasm module exporting the C ABI.
mkdir -p "$OUT_DIR"
emcc -O3 \
  -Wl,--start-group \
    "$BUILD_DIR/libzs_dwf_toolkit_native.a" \
    "$BUILD_DIR/oda/lib/libDwfToolkit.a" \
    "$BUILD_DIR/oda/lib/libWhipTk.a" \
    "$BUILD_DIR/oda/lib/libW3dTk.a" \
    "$BUILD_DIR/oda/lib/libDwfCore.a" \
    "$BUILD_DIR/liboda_jpeg.a" \
    "$BUILD_DIR/zlib/libz.a" \
    "$BUILD_DIR/freetype/libfreetype.a" \
  -Wl,--end-group \
  -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_NAME=ZsDwf -sEXIT_RUNTIME=0 \
  -sEXPORTED_FUNCTIONS=_zs_dwf_render_dwf_pdf,_zs_dwf_render_page,_zs_dwf_render_page_ex,_zs_w2d_render_file,_zs_dwf_get_info_json,_zs_dwf_get_last_error,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,FS,stringToNewUTF8,UTF8ToString \
  -o "$OUT_DIR/zs_dwf.js"

echo "Wrote $OUT_DIR/zs_dwf.js + zs_dwf.wasm"
