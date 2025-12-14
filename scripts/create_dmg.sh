#!/bin/bash
set -e

BUILD_DIR="$1"
VERSION="$2"

APP_NAME="DeadEditor"
DMG_NAME="${APP_NAME}-${VERSION}-macos"
DMG_PATH="${BUILD_DIR}/${DMG_NAME}.dmg"
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
TEMP_DIR="${BUILD_DIR}/dmg_temp"
VOLUME_NAME="${APP_NAME} ${VERSION}"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: ${APP_BUNDLE} not found. Build the app first."
    exit 1
fi

rm -rf "$TEMP_DIR"
rm -f "$DMG_PATH"

mkdir -p "$TEMP_DIR"

cp -R "$APP_BUNDLE" "$TEMP_DIR/"

ln -s /Applications "$TEMP_DIR/Applications"

if command -v create-dmg &> /dev/null; then
    create-dmg \
        --volname "$VOLUME_NAME" \
        --volicon "${BUILD_DIR}/${APP_NAME}.icns" \
        --window-pos 200 120 \
        --window-size 600 400 \
        --icon-size 100 \
        --icon "${APP_NAME}.app" 150 190 \
        --icon "Applications" 450 190 \
        --hide-extension "${APP_NAME}.app" \
        --app-drop-link 450 190 \
        "$DMG_PATH" \
        "$TEMP_DIR"
else
    TEMP_DMG="${BUILD_DIR}/temp_${DMG_NAME}.dmg"
    
    hdiutil create -srcfolder "$TEMP_DIR" \
        -volname "$VOLUME_NAME" \
        -fs HFS+ \
        -fsargs "-c c=64,a=16,e=16" \
        -format UDRW \
        "$TEMP_DMG"
    
    DEVICE=$(hdiutil attach -readwrite -noverify "$TEMP_DMG" | grep -E '^/dev/' | sed 1q | awk '{print $1}')
    MOUNT_POINT="/Volumes/${VOLUME_NAME}"
    
    sleep 2
    
    echo '
    tell application "Finder"
        tell disk "'"${VOLUME_NAME}"'"
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set the bounds of container window to {400, 100, 1000, 500}
            set viewOptions to the icon view options of container window
            set arrangement of viewOptions to not arranged
            set icon size of viewOptions to 100
            set position of item "'"${APP_NAME}.app"'" of container window to {150, 190}
            set position of item "Applications" of container window to {450, 190}
            close
            open
            update without registering applications
            delay 2
        end tell
    end tell
    ' | osascript
    
    sync
    
    hdiutil detach "$DEVICE"
    
    hdiutil convert "$TEMP_DMG" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH"
    
    rm -f "$TEMP_DMG"
fi

rm -rf "$TEMP_DIR"

echo "Created DMG: $DMG_PATH"

