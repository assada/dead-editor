#!/bin/bash
set -e

BUILD_DIR="$1"
SOURCE_DIR="$2"
VERSION="$3"

APP_NAME="DeadEditor"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"

rm -rf "$APP_BUNDLE"

mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

cp "$BUILD_DIR/$APP_NAME" "$APP_BUNDLE/Contents/MacOS/"

cp "$BUILD_DIR/$APP_NAME.icns" "$APP_BUNDLE/Contents/Resources/"

cp "$BUILD_DIR/JetBrainsMonoNLNerdFont-Regular.ttf" "$APP_BUNDLE/Contents/Resources/"
cp "$BUILD_DIR/icon.bmp" "$APP_BUNDLE/Contents/Resources/"

sed "s/@VERSION@/$VERSION/g" "$SOURCE_DIR/packaging/macos/Info.plist.in" > "$APP_BUNDLE/Contents/Info.plist"

echo "APPL????" > "$APP_BUNDLE/Contents/PkgInfo"

echo "Created $APP_BUNDLE"

