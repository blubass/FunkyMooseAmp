#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="/Users/uwearthurfelchle/Desktop/FunkyMooseAmp_PROJECT_FULL_CMakeOldStyle"
JUCE_PREFIX="/Users/uwearthurfelchle/Developer/JUCE/install"

BUILD_DIR="$PROJECT_ROOT/build"
# Note: CMake target name in CMakeLists.txt is FUNKY_MOOSE_AMP
ARTEFACTS="$BUILD_DIR/FUNKY_MOOSE_AMP_artefacts/Release"

SIGN_SCRIPT="/Users/uwearthurfelchle/Juce/ganzneu/sign_all_macos.sh"
KIT="/Users/uwearthurfelchle/Juce/ganzneu/FunkyMoose_ReleaseKit_0.95"
BG="$KIT/FunkyMoose_DMG_Background_Walnut.png"
STAGE="$PROJECT_ROOT/DMG_STAGING"

# Name configuration
VERSION="1.0.1-Alpha"
DMG_NAME="FunkyMooseAmp_v${VERSION}"
VOL_NAME="Funky Moose Amp ${VERSION}"

echo "🚀 Starting Official Release Build v${VERSION}..."

cd "$PROJECT_ROOT"

# 1) Configure (Release)
echo "-> Configuring..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$JUCE_PREFIX"

# 2) Build
echo "-> Building..."
cmake --build "$BUILD_DIR" --config Release --parallel

# 3) Sign
echo "-> Signing..."
bash "$SIGN_SCRIPT"

# 4) Stage
echo "-> Staging..."
rm -rf "$STAGE"
mkdir -p "$STAGE"

# Find files (flexible naming)
cp -R "$ARTEFACTS/VST3/"*.vst3 "$STAGE/"
cp -R "$ARTEFACTS/AU/"*.component "$STAGE/"
cp -R "$ARTEFACTS/Standalone/"*.app "$STAGE/"

# Manuals from Kit
cp -f "$KIT/FunkyMoose_Manual_EN.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$KIT/FunkyMoose_Manual_DE.pdf" "$STAGE/" 2>/dev/null || true

# 5) Create DMG (Internal logic to avoid dependency on create_dmg.sh naming)
echo "-> Creating DMG: ${DMG_NAME}.dmg"

TMP_DMG="${PROJECT_ROOT}/${DMG_NAME}_temp.dmg"
OUT_DMG="${PROJECT_ROOT}/${DMG_NAME}.dmg"

rm -f "${TMP_DMG}" "${OUT_DMG}"

# Applications link
ln -sfn /Applications "${STAGE}/Applications"

# Background
mkdir -p "${STAGE}/.background"
cp -f "${BG}" "${STAGE}/.background/background.png"

# Create RW dmg
hdiutil create -fs HFS+ -volname "${VOL_NAME}" -srcfolder "${STAGE}" -ov -format UDRW "${TMP_DMG}" >/dev/null

# Mount
MOUNT_POINT=$(hdiutil attach -readwrite -noverify -noautoopen "${TMP_DMG}" | tail -n 1 | awk -F $'\t' '{print $NF}')
echo "Mounted at: ${MOUNT_POINT}"

# Finder layout
osascript <<EOF2 || true
tell application "Finder"
  tell disk "${VOL_NAME}"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {200, 150, 980, 620}
    set viewOptions to the icon view options of container window
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 96
    set background picture of viewOptions to file ".background:background.png"

    try
      set position of item "Applications" to {740, 360}
    end try

    try
      set position of item "Funky Moose Amp.vst3" to {240, 240}
    end try
    try
      set position of item "Funky Moose Amp.component" to {420, 240}
    end try
    try
      set position of item "Funky Moose Amp.app" to {600, 240}
    end try

    try
      set position of item "FunkyMoose_Manual_EN.pdf" to {320, 430}
    end try
    try
      set position of item "FunkyMoose_Manual_DE.pdf" to {500, 430}
    end try

    close
    open
    update without registering applications
    delay 2
  end tell
end tell
EOF2

hdiutil detach "${MOUNT_POINT}" >/dev/null || hdiutil detach -force "${MOUNT_POINT}" >/dev/null

# Convert
hdiutil convert "${TMP_DMG}" -format UDZO -imagekey zlib-level=9 -o "${OUT_DMG}" >/dev/null
rm -f "${TMP_DMG}"

echo "✅ DMG ready: ${OUT_DMG}"
