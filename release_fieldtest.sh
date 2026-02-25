#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="/Users/uwearthurfelchle/Juce/ganzneu/FunkyMooseAmp_PROJECT_FULL_CMakeOldStyle"
JUCE_PREFIX="/Users/uwearthurfelchle/Developer/JUCE/install"

BUILD_DIR="$PROJECT_ROOT/build"
ARTEFACTS="$BUILD_DIR/FUNKY_MOOSE_AMP_VST3_artefacts/Release"

SIGN_SCRIPT="/Users/uwearthurfelchle/Juce/ganzneu/sign_all_macos.sh"

KIT="/Users/uwearthurfelchle/Juce/ganzneu/FunkyMoose_ReleaseKit_0.95"
BG="$KIT/FunkyMoose_DMG_Background_Walnut.png"
DMG_MAKER="$KIT/create_dmg.sh"
STAGE="$PROJECT_ROOT/DMG_STAGING"

cd "$PROJECT_ROOT"

# configure only if needed
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$JUCE_PREFIX"
fi

# incremental build
cmake --build "$BUILD_DIR" --config Release

# sign
bash "$SIGN_SCRIPT"

# stage
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$ARTEFACTS/VST3/Funky Moose Amp.vst3" "$STAGE/"
cp -R "$ARTEFACTS/AU/Funky Moose Amp.component" "$STAGE/"
cp -R "$ARTEFACTS/Standalone/Funky Moose Amp.app" "$STAGE/"

cp -f "$KIT/FunkyMoose_Manual_EN.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$KIT/FunkyMoose_Manual_DE.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$KIT/FunkyMoose_FeedbackForm.pdf" "$STAGE/" 2>/dev/null || true
cp -f "$KIT/TESTER_README.txt" "$STAGE/" 2>/dev/null || true

chmod +x "$DMG_MAKER"
bash "$DMG_MAKER" "$STAGE" "$BG"

echo "✅ DMG ready: $PROJECT_ROOT/FunkyMooseAmp_FieldTest_0.95.dmg"
