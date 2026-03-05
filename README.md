# Funky Moose Amp
![Funky Moose Amp](https://raw.githubusercontent.com/blubass/FunkyMooseAmp/main/Plugin%20Screenshot.png)

A highly versatile, state-of-the-art Bass Amplifier Plugin, specifically designed to bring the funk! Built with modern JUCE frameworks and featuring a dynamic, interactive UI.

## Features Overview

### 1. **Tuner & Input Section**
* **High-Precision Tuner:** Always visible at the top left. Instantly reacts to your input signal.
* **Input Stage:** Control the amount of signal hitting the amp head.

### 2. **Amp & Tone Module**
The heart of the Moose. A custom-modeled solid-state preamp with optional tube characteristics.
* **Gain:** Drive the preamp section from crystal clean to gritty overdrive.
* **Tone Stack (Bass / Mid / Treble):** Highly responsive analog-modeled EQs.
* **Volume:** Master output volume of the preamp section.
* **Tube Saturation (Toggle):** Engages an asymmetric tube-style saturation curve with dynamic harmonic generation.
* **Slap Mode (Toggle):** A mid-scoop (500Hz) paired with a bass & treble boost (+5dB), specifically tailored for modern slap-bass tones.
* **Low Cut (Toggle):** HPF at 40Hz to clean out subsonic rumble and maintain tight low-end.
* **Auto Gain:** Automatically compensates overall volume changes when adjusting the EQ knobs.

### 3. **Smart Compressor**
An ultra-clean, transparent VCA-style compressor.
* **Controls:** Input (Drive), Threshold, Makeup Gain, Attack, Release.
* **Ratio Selection:** 4:1, 8:1, 12:1, and 20:1 (Limiting).
* **Punch Circuit (Toggle):** Enhances the attack phase and tightens the low-end grip, making bass lines cut through any mix.
* **Auto-Makeup (AUTO):** Analyzes gain reduction and automatically corrects output level.
* **Dry/Wet Mix:** Built-in parallel compression without phase issues.
* **Gain Reduction Meter:** Real-time visual feedback of compression amount.

### 4. **Effects Section (ModFX)**
Built-in high-quality effects, routeable in series or parallel.
* **Octaver:** Sub-bass generator. Blends analog-style -1 Octave and +1 Octave signals.
* **Envelope Filter (Auto-Wah):** Touch-sensitive filter. Controls: Attack, Decay, Range.
* **Phaser:** Classic 4-stage optical phaser with Rate, Colour (Feedback), and Mix controls.
* **Chorus:** Lush analog-style stereo bucket-brigade chorus.
* **Parallel Mode (Toggle):** Move the Chorus & Phaser into a parallel signal path alongside the dry/amp signal to retain maximum low-end punch while adding width.

### 5. **Master Section & Custom Cab IRs**
* **Master Out:** Controls the final plugin output.
* **Mono Maker:** Sums all frequencies below a certain threshold to mono, ensuring phase-coherency in the crucial low registers.
* **Master Dry/Wet:** Mix the entire processed signal with your DI signal.
* **CAB IR Loader (CAB: OFF/ON):** Load custom Impulse Responses. Includes built-in custom "Funky Moose" Bass Cab responses. Easily switchable and mixable (IR Mix).

### 6. **The "Funky Moose" Interactive UI**
The Elch (Moose) is not just a logo; it's a dynamic visualizer of your tone!
* **Level Visualization:** The Moose reacts to both your input and output RMS levels.
* **Compression Feedback:** Heavy gain reduction affects the background texture and glow.
* **Sunglasses Glow:** Engaging the "Punch" circuit on the compressor activates a bright neon glow on the Moose's sunglasses.
* **State-of-the-Art Look & Feel:** Features a skeuomorphic aesthetic with realistic shadows, glowing toggle lamps (States), and a custom Pop-up preset system.

## Presets & MIDI
* **Presets:** Includes an entire Factory Bank covering Clean DI tones, Vintage Motown, Modern Slap, Synth Bass, and Heavy Overdrives. Supports saving User Banks.
* **MIDI Learn:** Just click "MIDI LRN" and turn a knob on your physical controller (e.g., Akai MPK Mini) to map CC parameters instantly to any knob.

## Build Instructions
Can be built as VST3, AU, and a Standalone App using CMake.
```bash
cmake -B build
cmake --build build --target FUNKY_MOOSE_AMP_Standalone
```
