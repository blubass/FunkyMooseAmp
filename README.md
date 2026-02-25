Funky Moose Amp – FULL CMake Project (VST3 + AU + Standalone)

This is a complete, buildable JUCE CMake project skeleton with:
- New panel textures (panel_base + panel_wear_v2)
- Knob overlays (metal_tile, knob_spec, knob_scratches)
- Elch RMS glow animation (Preset B smoothing)
- Standalone app icon (FunkyMoose.icns)

IMPORTANT
1) You must have JUCE built/installed and adjust JUCE_DIR in CMakeLists.txt.
2) This is a UI+project skeleton. Merge your real DSP & full parameter set into PluginProcessor.

Build:
  mkdir build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  cmake --build . --config Release

Outputs (macOS):
- Standalone: Funky Moose Amp.app
- AU: Funky Moose Amp.component
- VST3: Funky Moose Amp.vst3


## New Elch Asset
- Includes `Assets/elch_new.png` as BinaryData and sets it on the ElchComponent in PluginEditor.

## Future Plans (Upcoming)
1. **DMG Installer Creation**
   - Create a fully branded `.dmg` installer for easy distribution.
   - **Design Ideas**:
     - Custom background image ("Plug me in!" graphic with cable pointing to the folder).
     - Custom volume icon (Elch Head) for the mounted disk image.
     - Custom folder layout (Funky Moose on the left, Amp/Speaker on the right).
   - Use `hdiutil` and AppleScript to automate the build.
