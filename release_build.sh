#!/usr/bin/env bash
set -euo pipefail

# -----------------------------
# Funky Moose Amp - Release ZIP
# -----------------------------

# Adjust these if needed:
PROJECT_NAME="FunkyMooseAmp"
TARGET_ARTEFACTS_DIR="build/FUNKY_MOOSE_AMP_VST3_artefacts/Release"
JUCE_INSTALL_PREFIX="/Users/uwearthurfelchle/Developer/JUCE/install"

# Output folder
REL_DIR="Release"
PKG_DIR="${REL_DIR}/${PROJECT_NAME}_macOS_universal"
ZIP_NAME="${PROJECT_NAME}_macOS_universal.zip"

echo "== Funky Moose Release builder =="
echo "JUCE prefix: ${JUCE_INSTALL_PREFIX}"
echo

# 1) Clean build dir
echo "-> Cleaning build folder"
rm -rf build "${REL_DIR}"
mkdir -p "${PKG_DIR}"

# 2) Configure + Build
echo "-> Configuring (Release)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${JUCE_INSTALL_PREFIX}"

echo "-> Building (Release)"
cmake --build build --config Release

# 3) Collect artefacts
echo "-> Collecting artefacts"

# VST3
if [ -d "${TARGET_ARTEFACTS_DIR}/VST3" ]; then
  mkdir -p "${PKG_DIR}/VST3"
  cp -R "${TARGET_ARTEFACTS_DIR}/VST3/"*.vst3 "${PKG_DIR}/VST3/" || true
fi

# AU
if [ -d "${TARGET_ARTEFACTS_DIR}/AU" ]; then
  mkdir -p "${PKG_DIR}/AU"
  cp -R "${TARGET_ARTEFACTS_DIR}/AU/"*.component "${PKG_DIR}/AU/" || true
fi

# Standalone
if [ -d "${TARGET_ARTEFACTS_DIR}/Standalone" ]; then
  mkdir -p "${PKG_DIR}/Standalone"
  cp -R "${TARGET_ARTEFACTS_DIR}/Standalone/"*.app "${PKG_DIR}/Standalone/" || true
fi

# 4) Add install instructions
echo "-> Writing INSTALL_macOS.txt"
cat > "${PKG_DIR}/INSTALL_macOS.txt" <<'EOF'
Funky Moose Amp — macOS Installation (Freeware / no Apple notarization)

1) Copy the plugins to your user plugin folders:
   VST3:
     ~/Library/Audio/Plug-Ins/VST3/
   AU (Logic):
     ~/Library/Audio/Plug-Ins/Components/

   Optional standalone app:
     Put the .app anywhere (e.g. /Applications/)

2) If macOS blocks it (Gatekeeper / quarantine), run these commands in Terminal:

   # VST3
   xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3"

   # AU
   xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components"

   # Standalone (if you use it)
   xattr -dr com.apple.quarantine "/Applications"

   Then restart your DAW (Logic/Cubase) and rescan plugins if needed.

Notes:
- Some systems may still show a warning because this build is not notarized.
- Right-click -> Open can help for the standalone app.
EOF

# 5) Add README stub (optional)
echo "-> Writing README_RELEASE.txt"
cat > "${PKG_DIR}/README_RELEASE.txt" <<'EOF'
Funky Moose Amp — Release Package

Contents:
- VST3/  (Cubase, Reaper, etc.)
- AU/    (Logic Pro)
- Standalone/ (optional app)
- INSTALL_macOS.txt

Build:
- Universal Binary (arm64 + x86_64)
- Minimum macOS: 11.0
EOF

# 6) Make ZIP (keep parent folder)
echo "-> Creating ZIP: ${ZIP_NAME}"
(
  cd "${REL_DIR}"
  /usr/bin/ditto -c -k --keepParent "${PROJECT_NAME}_macOS_universal" "${ZIP_NAME}"
)

echo
echo "✅ Done!"
echo "ZIP created at: ${REL_DIR}/${ZIP_NAME}"
echo "Package folder: ${PKG_DIR}"