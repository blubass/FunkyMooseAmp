#!/usr/bin/env bash
# Official Release Script for Funky Moose Amp v1.1.0
set -euo pipefail

# ---- CONFIGURATION ----
VERSION="1.1.0"
PROJECT_NAME="FunkyMooseAmp"
VOL_NAME="Funky Moose Amp"
DMG_NAME="${PROJECT_NAME}_v${VERSION}_macOS"

# Paths
PROJECT_ROOT=$(pwd)
BUILD_DIR="${PROJECT_ROOT}/build"
RELEASE_DIR="${PROJECT_ROOT}/Release"
STAGE="${PROJECT_ROOT}/DMG_STAGING"
JUCE_PREFIX="/Users/uwearthurfelchle/Developer/JUCE/install"

# Assets
BG_IMG="${PROJECT_ROOT}/dmg_background.png"
ICON_IMG="${PROJECT_ROOT}/Source/Assets/FunkyMoose.icns"
ENTITLEMENTS="${PROJECT_ROOT}/Source/mac_entitlements.plist"

# Artifacts
ARTIFACTS="${BUILD_DIR}/FUNKY_MOOSE_AMP_artefacts/Release"

echo "🚀 Starting Official Release Build v${VERSION}..."
echo "Project Root: ${PROJECT_ROOT}"

# 1) Clean environment
echo "-> Cleaning previous builds..."
rm -rf "${BUILD_DIR}" "${RELEASE_DIR}" "${STAGE}"
mkdir -p "${RELEASE_DIR}"
mkdir -p "${STAGE}"

# 2) Configure & Build (Universal Binary Release)
echo "-> Configuring CMake..."
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${JUCE_PREFIX}" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

echo "-> Building (Release)..."
cmake --build "${BUILD_DIR}" --config Release --parallel

# 3) Signing (Ad-hoc signing with entitlements for Standalone)
# Note: VST3 and AU bundles are signed but generally need notarization for Gatekeeper.
# This script handles the basic ad-hoc signing for local safety/standalone input.
echo "-> Signing artifacts..."
codesign --force --deep --sign - --entitlements "${ENTITLEMENTS}" --options runtime "${ARTIFACTS}/Standalone/Funky Moose Amp.app"

# 4) Stage DMG Files
echo "-> Staging files for DMG..."

# Copy Plugins
cp -R "${ARTIFACTS}/VST3/"*.vst3 "${STAGE}/" 2>/dev/null || echo "⚠️ VST3 not found"
cp -R "${ARTIFACTS}/AU/"*.component "${STAGE}/" 2>/dev/null || echo "⚠️ AU not found"
cp -R "${ARTIFACTS}/Standalone/"*.app "${STAGE}/" 2>/dev/null || echo "⚠️ Standalone not found"

# Copy Manuals/Guides (if they exist)
[ -f "INSTALL_GUIDE_EN.txt" ] && cp "INSTALL_GUIDE_EN.txt" "${STAGE}/"
[ -f "INSTALL_GUIDE_DE.txt" ] && cp "INSTALL_GUIDE_DE.txt" "${STAGE}/"

# Folder Aliases for Drag-and-Drop
ln -s /Library/Audio/Plug-Ins/VST3 "${STAGE}/VST3_Folder"
ln -s /Library/Audio/Plug-Ins/Components "${STAGE}/Components_Folder"
ln -s /Applications "${STAGE}/Applications_Folder"

# 5) Create DMG
echo "-> Creating DMG..."
TMP_DMG="${RELEASE_DIR}/${DMG_NAME}_temp.dmg"
OUT_DMG="${RELEASE_DIR}/${DMG_NAME}.dmg"

# Background preparation
mkdir -p "${STAGE}/.background"
if [ -f "${BG_IMG}" ]; then
    sips -z 700 1200 "${BG_IMG}" --out "${STAGE}/.background/background.tiff" >/dev/null
else
    echo "⚠️ DMG Background not found at ${BG_IMG}, using plain background."
fi

# Volume Icon
cp -f "${ICON_IMG}" "${STAGE}/.VolumeIcon.icns"
SetFile -a C "${STAGE}"

# Create the disk image
hdiutil create -fs HFS+ -volname "${VOL_NAME}" -srcfolder "${STAGE}" -ov -format UDRW "${TMP_DMG}" >/dev/null

# Mount and Apply Finder Styles
MOUNT_POINT=$(hdiutil attach -readwrite -noverify -noautoopen "${TMP_DMG}" | tail -n 1 | awk -F $'\t' '{print $NF}')
echo "Mounted at: ${MOUNT_POINT}"

cp -f "${ICON_IMG}" "${MOUNT_POINT}/.VolumeIcon.icns"
SetFile -a V "${MOUNT_POINT}/.VolumeIcon.icns"
SetFile -a C "${MOUNT_POINT}"

sleep 2

# AppleScript to set Finder layout
osascript <<EOF || true
tell application "Finder"
  tell disk "${VOL_NAME}"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set statusbar visible of container window to false
    set the bounds of container window to {100, 100, 1300, 800}
    set viewOptions to the icon view options of container window
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 100
    set label position of viewOptions to bottom
    set text size of viewOptions to 14
    
    # Set background if tiff exists
    try
        set background picture of viewOptions to file ".background:background.tiff"
    end try

    # Positions
    set position of item "Funky Moose Amp.vst3" to {250, 200}
    set position of item "Funky Moose Amp.component" to {500, 200}
    set position of item "Funky Moose Amp.app" to {750, 200}

    set position of item "VST3_Folder" to {250, 450}
    set position of item "Components_Folder" to {500, 450}
    set position of item "Applications_Folder" to {750, 450}

    set position of item "INSTALL_GUIDE_EN.txt" to {1000, 250}
    set position of item "INSTALL_GUIDE_DE.txt" to {1000, 450}

    update without registering applications
    delay 2
    close
  end tell
end tell
EOF

SetFile -a V "${MOUNT_POINT}/.background"
hdiutil detach "${MOUNT_POINT}" >/dev/null || hdiutil detach -force "${MOUNT_POINT}" >/dev/null

# Final Convert
hdiutil convert "${TMP_DMG}" -format UDZO -imagekey zlib-level=9 -o "${OUT_DMG}" >/dev/null
rm -f "${TMP_DMG}"

# 6) Create a ZIP fallback
echo "-> Creating ZIP fallback..."
(
  cd "${STAGE}"
  rm -f *_Folder Applications_Folder  # Don't include aliases in ZIP
  zip -r "${RELEASE_DIR}/${PROJECT_NAME}_v${VERSION}_macOS_Universal.zip" .
)

echo "✅ Build Complete!"
echo "📍 DMG: ${OUT_DMG}"
echo "📍 ZIP: ${RELEASE_DIR}/${PROJECT_NAME}_v${VERSION}_macOS_Universal.zip"
echo "🎉 Version ${VERSION} is ready for export."
