#!/usr/bin/env bash
# Official Release Script for Funky Moose Amp
set -euo pipefail

PROJECT_ROOT="/Users/uwearthurfelchle/Desktop/FunkyMooseAmp_PROJECT_FULL_CMakeOldStyle"
JUCE_PREFIX="/Users/uwearthurfelchle/Developer/JUCE/install"

BUILD_DIR="$PROJECT_ROOT/build"
ARTEFACTS="$BUILD_DIR/FUNKY_MOOSE_AMP_artefacts/Release"

SIGN_SCRIPT="/Users/uwearthurfelchle/Juce/ganzneu/sign_all_macos.sh"
KIT="/Users/uwearthurfelchle/Juce/ganzneu/FunkyMoose_ReleaseKit_0.95"
# New cleaner background
BG="/Users/uwearthurfelchle/.gemini/antigravity/brain/8232f804-49d7-40a7-940a-fb608d201f77/clean_walnut_dmg_background_1772360582595.png"
STAGE="$PROJECT_ROOT/DMG_STAGING"

# Name configuration
VERSION="1.0.1"
DMG_NAME="FunkyMooseAmp_v${VERSION}"
VOL_NAME="Funky Moose Amp"

echo "🚀 Starting Official Release Build v${VERSION}..."

cd "$PROJECT_ROOT"

# 1) Configure (Release)
echo "-> Configuring..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$JUCE_PREFIX"

# 2) Build
echo "-> Building..."
cmake --build "$BUILD_DIR" --config Release --parallel

# 3) Sign (Universal Binary signing)
echo "-> Signing..."
if [ -f "$SIGN_SCRIPT" ]; then
    bash "$SIGN_SCRIPT"
else
    echo "⚠️ Signing script not found, skipping..."
fi

# 4) Stage
echo "-> Staging Files..."
rm -rf "$STAGE"
mkdir -p "$STAGE"

# Copy Plugins
cp -R "$ARTEFACTS/VST3/"*.vst3 "$STAGE/" 2>/dev/null || echo "⚠️ VST3 not found"
cp -R "$ARTEFACTS/AU/"*.component "$STAGE/" 2>/dev/null || echo "⚠️ AU not found"
cp -R "$ARTEFACTS/Standalone/"*.app "$STAGE/" 2>/dev/null || echo "⚠️ Standalone not found"

# Copy Manuals and Install Guides
cp -f "$KIT/FunkyMoose_Manual_EN.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$KIT/FunkyMoose_Manual_DE.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$PROJECT_ROOT/INSTALL_GUIDE_EN.txt" "$STAGE/"
cp -f "$PROJECT_ROOT/INSTALL_GUIDE_DE.txt" "$STAGE/"

# 5) Create DMG
echo "-> Creating DMG: ${DMG_NAME}.dmg"

TMP_DMG="${PROJECT_ROOT}/${DMG_NAME}_temp.dmg"
OUT_DMG="${PROJECT_ROOT}/${DMG_NAME}.dmg"

rm -f "${TMP_DMG}" "${OUT_DMG}"

# Prepare folders for drag-and-drop installation (Aliases)
ln -s /Library/Audio/Plug-Ins/VST3 "$STAGE/VST3_Folder"
ln -s /Library/Audio/Plug-Ins/Components "$STAGE/Components_Folder"
ln -s /Applications "$STAGE/Applications_Folder"

# Prepare Background (Scale specifically to 1200x700 for the window)
mkdir -p "${STAGE}/.background"
sips -z 700 1200 "${BG}" --out "${STAGE}/.background/background.tiff" >/dev/null

# Volume Icon
cp -f "Source/Assets/FunkyMoose.icns" "${STAGE}/.VolumeIcon.icns"
SetFile -a C "${STAGE}"

# Create RW dmg
hdiutil create -fs HFS+ -volname "${VOL_NAME}" -srcfolder "${STAGE}" -ov -format UDRW "${TMP_DMG}" >/dev/null

# Mount
MOUNT_POINT=$(hdiutil attach -readwrite -noverify -noautoopen "${TMP_DMG}" | tail -n 1 | awk -F $'\t' '{print $NF}')
echo "Mounted at: ${MOUNT_POINT}"

# Apply Volume Icon to mounted volume (crucial for it to show up on the mounted drive)
cp -f "Source/Assets/FunkyMoose.icns" "${MOUNT_POINT}/.VolumeIcon.icns"
SetFile -a V "${MOUNT_POINT}/.VolumeIcon.icns"
SetFile -a C "${MOUNT_POINT}"

# Finder layout
sleep 3
osascript <<EOF2 || true
tell application "Finder"
  tell disk "${VOL_NAME}"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    # Match the 1200x700 background exactly
    set the bounds of container window to {100, 100, 1300, 800}
    set viewOptions to the icon view options of container window
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 100
    set label position of viewOptions to bottom
    set text size of viewOptions to 14
    
    # Setting the background
    set background picture of viewOptions to file ".background:background.tiff"

    # Row 1: The Plugins (Top center-ish)
    set position of item "Funky Moose Amp.vst3" to {250, 200}
    set position of item "Funky Moose Amp.component" to {500, 200}
    set position of item "Funky Moose Amp.app" to {750, 200}

    # Row 2: The Target Folders (Below their plugins)
    set position of item "VST3_Folder" to {250, 400}
    set position of item "Components_Folder" to {500, 400}
    set position of item "Applications_Folder" to {750, 400}

    # Manuals and Guides (To the right)
    set position of item "FunkyMoose_Manual_EN.pdf" to {1000, 150}
    set position of item "FunkyMoose_Manual_DE.pdf" to {1000, 280}
    set position of item "INSTALL_GUIDE_EN.txt" to {1000, 450}
    set position of item "INSTALL_GUIDE_DE.txt" to {1000, 580}

    update without registering applications
    delay 5
    close
  end tell
end tell
EOF2

# Final bit: Hide background folder
SetFile -a V "${MOUNT_POINT}/.background"

hdiutil detach "${MOUNT_POINT}" >/dev/null || hdiutil detach -force "${MOUNT_POINT}" >/dev/null

# Convert to final compressed DMG
hdiutil convert "${TMP_DMG}" -format UDZO -imagekey zlib-level=9 -o "${OUT_DMG}" >/dev/null
rm -f "${TMP_DMG}"

echo "✅ Official Release DMG created: ${OUT_DMG}"
echo "🎉 Version ${VERSION} is ready for distribution!"
