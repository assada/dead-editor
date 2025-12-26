#!/bin/bash
set -e

BUILD_DIR="$1"
SOURCE_DIR="$2"
VERSION="$3"

APP_NAME="DeadEditor"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
EXECUTABLE="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"

rm -rf "$APP_BUNDLE"

mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"
mkdir -p "$FRAMEWORKS_DIR"

cp "$BUILD_DIR/$APP_NAME" "$EXECUTABLE"

cp "$BUILD_DIR/$APP_NAME.icns" "$APP_BUNDLE/Contents/Resources/"

cp "$BUILD_DIR/JetBrainsMonoNLNerdFont-Regular.ttf" "$APP_BUNDLE/Contents/Resources/"
cp "$BUILD_DIR/icon.bmp" "$APP_BUNDLE/Contents/Resources/"

sed "s/@VERSION@/$VERSION/g" "$SOURCE_DIR/packaging/macos/Info.plist.in" > "$APP_BUNDLE/Contents/Info.plist"

echo "APPL????" > "$APP_BUNDLE/Contents/PkgInfo"

bundle_dylib() {
    local dylib_path="$1"
    local dylib_name
    dylib_name=$(basename "$dylib_path")
    
    if [[ "$dylib_path" == /usr/lib/* ]] || [[ "$dylib_path" == /System/* ]]; then
        return
    fi
    
    if [[ -f "$FRAMEWORKS_DIR/$dylib_name" ]]; then
        return
    fi
    
    echo "Bundling: $dylib_path -> $FRAMEWORKS_DIR/$dylib_name"
    cp "$dylib_path" "$FRAMEWORKS_DIR/$dylib_name"
    chmod 644 "$FRAMEWORKS_DIR/$dylib_name"
    
    local sub_dylibs
    sub_dylibs=$(otool -L "$FRAMEWORKS_DIR/$dylib_name" | tail -n +2 | awk '{print $1}' | grep -v "^/usr/lib" | grep -v "^/System" | grep -v "@" || true)
    for sub_dylib in $sub_dylibs; do
        if [[ -f "$sub_dylib" ]]; then
            bundle_dylib "$sub_dylib"
        fi
    done
}

fix_dylib_paths() {
    local target="$1"
    
    local dylibs
    dylibs=$(otool -L "$target" | tail -n +2 | awk '{print $1}')
    
    for dylib in $dylibs; do
        local dylib_name
        dylib_name=$(basename "$dylib")
        
        if [[ "$dylib" == /usr/lib/* ]] || [[ "$dylib" == /System/* ]] || [[ "$dylib" == @* ]]; then
            continue
        fi
        
        if [[ -f "$FRAMEWORKS_DIR/$dylib_name" ]]; then
            echo "Fixing path in $(basename "$target"): $dylib -> @executable_path/../Frameworks/$dylib_name"
            install_name_tool -change "$dylib" "@executable_path/../Frameworks/$dylib_name" "$target"
        fi
    done
}

set_dylib_id() {
    local dylib="$1"
    local dylib_name
    dylib_name=$(basename "$dylib")
    install_name_tool -id "@executable_path/../Frameworks/$dylib_name" "$dylib"
}

echo "Bundling dynamic libraries..."

dylibs=$(otool -L "$EXECUTABLE" | tail -n +2 | awk '{print $1}' | grep -v "^/usr/lib" | grep -v "^/System" | grep -v "@" || true)
for dylib in $dylibs; do
    if [[ -f "$dylib" ]]; then
        bundle_dylib "$dylib"
    fi
done

echo "Fixing library paths..."

fix_dylib_paths "$EXECUTABLE"

for fw in "$FRAMEWORKS_DIR"/*.dylib; do
    if [[ -f "$fw" ]]; then
        set_dylib_id "$fw"
        fix_dylib_paths "$fw"
    fi
done

echo "Verifying bundle..."
otool -L "$EXECUTABLE" | head -20

echo "Created $APP_BUNDLE"

