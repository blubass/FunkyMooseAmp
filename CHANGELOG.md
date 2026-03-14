# Changelog

All notable changes to this project will be documented in this file.

---

## v1.2.0

Improved stability, feature set and DSP behavior.

Added
- **Integrated High-Precision Tuner**: Graphical chromatic tuner for studio and live use.
- **Real-time Safe MIDI Learn**: Easy parameter mapping (optimized for Akai MPK Mini & others).
- **Skin System**: 9 selectable visual styles (Classic, Midnight, Vintage, Electric, Used Up, Bloody, Orange, Ampeg, Toxic).
- **DSP Safety Guards**: Intelligent culling of denormals/NaNs and soft-clipping to prevent "digital snow" artifacts.
- **Additional Parameter Smoothing**: Smoother transitions for gain and EQ shifts.

Changed
- **Internal DSP Optimizations**: Performance improvements for the Amp and CabSim modules.
- **Improved Standalone Logic**: Automatic mono-summing for instrument inputs to avoid one-sided signal/noise.
- **Polished UI**: Refined typography and layout for better readability.

Fixed
- **Signal Issues in Standalone**: Fixed cases where the standalone app wouldn't receive audio from specific hardware inputs.
- **DSP Glitches**: Resolved artifacts when combining high Octaver levels with Tube saturation.
- **Consistency**: Fixed various minor UI refresh and preset loading bugs.

---

## v1.1.0

Added
- Improved cabinet simulation (4x10 and 1x15 profiles).
- Tone shaping refinements for the slap mode.

Fixed
- GUI loading issues in specific VST3 hosts (scaling fixes).

---

## v1.0.0

Initial public release.

Features
- High-quality bass amp module with Tube/Slap modes.
- Sub-octave generator.
- Cabinet simulation.
- Standalone / VST3 / AU formats.
