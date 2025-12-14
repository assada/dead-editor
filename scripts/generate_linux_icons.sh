#!/bin/bash
set -e

INPUT="$1"
OUTPUT_DIR="$2"

SIZES=(16 24 32 48 64 128 256 512)

for SIZE in "${SIZES[@]}"; do
    mkdir -p "$OUTPUT_DIR/hicolor/${SIZE}x${SIZE}/apps"
    ffmpeg -y -i "$INPUT" -vf "scale=${SIZE}:${SIZE}" "$OUTPUT_DIR/hicolor/${SIZE}x${SIZE}/apps/deadedit.png" 2>/dev/null
done

mkdir -p "$OUTPUT_DIR/hicolor/scalable/apps"
cp "$INPUT" "$OUTPUT_DIR/hicolor/scalable/apps/deadedit.png"

echo "Generated Linux icons in $OUTPUT_DIR"

