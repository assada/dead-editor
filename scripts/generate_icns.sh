#!/bin/bash
set -e

INPUT="$1"
OUTPUT="$2"
ICONSET_DIR="${OUTPUT%.icns}.iconset"

rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"

SIZES=(16 32 64 128 256 512)

for SIZE in "${SIZES[@]}"; do
    SIZE_2X=$((SIZE * 2))
    sips -z "$SIZE" "$SIZE" "$INPUT" --out "$ICONSET_DIR/icon_${SIZE}x${SIZE}.png" >/dev/null 2>&1
    sips -z "$SIZE_2X" "$SIZE_2X" "$INPUT" --out "$ICONSET_DIR/icon_${SIZE}x${SIZE}@2x.png" >/dev/null 2>&1
done

iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT"
rm -rf "$ICONSET_DIR"

