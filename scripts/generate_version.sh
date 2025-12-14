#!/bin/sh
INPUT="$1"
OUTPUT="$2"
VERSION="$3"
SOURCE_ROOT="$4"

git_hash=$(git -C "$SOURCE_ROOT" describe --always --dirty 2>/dev/null || echo unknown)
sed -e "s/@PROJECT_VERSION@/$VERSION/g" -e "s/@VCS_TAG@/$git_hash/g" "$INPUT" > "$OUTPUT"
