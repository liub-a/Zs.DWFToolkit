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

echo "ODA-modified DWFToolkit extracted to: $TARGET"
