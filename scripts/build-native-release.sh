#!/usr/bin/env bash
#
# Builds the ODA native renderer + zs_dwf_worker for the CURRENT platform and
# stages them under artifacts/native/<rid>/ so `dotnet pack` picks them up.
# Run this once per target platform (on that platform): linux-x64, linux-arm64,
# osx-arm64, osx-x64.  Optionally packs the NuGet at the end.
#
# Usage:
#   scripts/build-native-release.sh            # build + stage for this platform
#   scripts/build-native-release.sh --pack     # also run dotnet pack
#   ZS_RID=linux-x64 scripts/build-native-release.sh   # override the detected RID
#
# Requirements: cmake, a C/C++ toolchain (g++/clang++), and unzip. All third-party
# libraries (FreeType, libtiff, zlib, libjpeg) are vendored; no system libs needed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# --- detect runtime identifier -------------------------------------------------
detect_rid() {
  local os arch
  case "$(uname -s)" in
    Linux)  os=linux ;;
    Darwin) os=osx ;;
    MINGW*|MSYS*|CYGWIN*) os=win ;;
    *) echo "unsupported OS: $(uname -s)" >&2; exit 1 ;;
  esac
  case "$(uname -m)" in
    x86_64|amd64) arch=x64 ;;
    arm64|aarch64) arch=arm64 ;;
    *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
  esac
  echo "${os}-${arch}"
}
RID="${ZS_RID:-$(detect_rid)}"

# --- native library file names per OS -----------------------------------------
case "$RID" in
  linux-*) LIB=libzs_dwf_toolkit_native.so;  WORKER=zs_dwf_worker ;;
  osx-*)   LIB=libzs_dwf_toolkit_native.dylib; WORKER=zs_dwf_worker ;;
  win-*)   LIB=zs_dwf_toolkit_native.dll;     WORKER=zs_dwf_worker.exe ;;
esac

echo ">> target RID: $RID"

# --- import the ODA toolkit source if needed ----------------------------------
if [[ ! -f "$ROOT/third_party/DWFToolkit-7.7/CMakeLists.txt" ]]; then
  echo ">> importing ODA DWF Toolkit source"
  "$ROOT/scripts/setup-oda-dwftoolkit.sh"
fi

# --- configure + build (bounded parallelism: WhipTk TUs are memory-hungry) -----
BUILD="$ROOT/build/native-release"
echo ">> configuring"
cmake -S "$ROOT/src/Zs.DWFToolkit.Native" -B "$BUILD" \
  -DZS_DWF_ENABLE_ODA_DWFTK=ON \
  -DZS_DWF_WITH_FREETYPE=ON \
  -DZS_DWF_WITH_TIFF=ON \
  -DCMAKE_BUILD_TYPE=Release

JOBS="${ZS_JOBS:-2}"
echo ">> building (-j $JOBS) — each WhipTk TU needs ~GBs of RAM; raise ZS_JOBS only if you have headroom"
cmake --build "$BUILD" --config Release -j "$JOBS" \
  --target zs_dwf_toolkit_native zs_dwf_worker

# --- stage artifacts into the committed prebuilt dir ---------------------------
# native-prebuilt/<rid>/ is checked in and used by `dotnet pack`. Re-run this only
# when the native code changes, then commit the updated binaries.
OUT="$ROOT/native-prebuilt/$RID"
mkdir -p "$OUT"
LIB_PATH="$(find "$BUILD" -name "$LIB" | head -1)"
WORKER_PATH="$(find "$BUILD" -name "$WORKER" | head -1)"
[[ -n "$LIB_PATH" ]] || { echo "native library $LIB not found in $BUILD" >&2; exit 1; }
cp "$LIB_PATH" "$OUT/"
[[ -n "$WORKER_PATH" ]] && cp "$WORKER_PATH" "$OUT/" || echo "WARN: worker not built (in-process rendering only)"

echo ">> staged:"
ls -la "$OUT"

# --- optional pack -------------------------------------------------------------
if [[ "${1:-}" == "--pack" ]]; then
  echo ">> dotnet pack"
  dotnet pack "$ROOT/src/Zs.DWFToolkit/Zs.DWFToolkit.csproj" -c Release -o "$ROOT/artifacts/nuget"
  ls -la "$ROOT/artifacts/nuget"/*.nupkg
fi

echo ">> done. RID=$RID staged at artifacts/native/$RID"
echo ">> to build other platforms, run this script on each, then: dotnet pack src/Zs.DWFToolkit/Zs.DWFToolkit.csproj -c Release -o artifacts/nuget"
