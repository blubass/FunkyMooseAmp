# Funky Moose Amp
![Funky Moose Amp](Source/Assets/screenshot_v120.png)

*(German version see [README_de.md](README_de.md))*

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

# Funky Moose Amp
![Funky Moose Amp](Source/Assets/screenshot_v120.png)

*(German version see [README_de.md](README_de.md))*

Funky Moose Amp is a modern, professional bass amplifier plugin designed for clarity, punch, and character. Built with JUCE and engineered for macOS (Intel & Apple Silicon), it combines a responsive solid‑state preamp, intelligent dynamics, creative modulation effects, and a distinctive interactive interface.

This is not a generic amp simulator. It is a tone-shaping instrument built for funk, groove, and expressive bass playing.

---

# Key Features

## 🎚 Input & Tuner Section
- **High‑Precision Tuner** – Always visible and instantly responsive.
- **Input Stage** – Control how hard the signal drives the preamp.

---

## 🔥 Amp & Tone Section
A custom-designed solid‑state preamp with optional tube-style saturation.

- **Gain** – From clean articulation to gritty drive.
- **Bass / Mid / Treble** – Musical, analog‑voiced tone stack.
- **Master Volume** – Preamp output control.
- **Tube Mode** – Asymmetric harmonic saturation for warmth and edge.
- **Slap Mode** – Modern mid‑scoop with enhanced lows and highs.
- **Low Cut** – 40 Hz high-pass filter for tighter low-end.
- **Auto Gain Compensation** – Maintains consistent perceived loudness when shaping tone.

---

## 🎛 Smart Compressor
Transparent VCA-style compression designed specifically for bass.

- **Drive, Threshold, Makeup**
- **Attack & Release**
- **Selectable Ratios** – 4:1, 8:1, 12:1, 20:1
- **Punch Mode** – Enhanced transient response and low-end control
- **Auto Makeup** – Intelligent level compensation
- **Dry/Wet Blend** – Parallel compression without phase artifacts
- **Gain Reduction Meter** – Real-time visual feedback

---

## 🎶 Modulation & Effects (ModFX)
High-quality built-in effects, usable in serial or partial parallel routing.

- **Octaver** – -1 and +1 octave blend
- **Envelope Filter** – Touch-sensitive auto-wah
- **Phaser** – 4-stage classic modulation
- **Chorus** – Analog-style stereo width
- **Parallel Mode** – Preserve low-end clarity while adding movement

---

## 🧱 Cabinet & Master Section

- **Master Output**
- **Mono Maker** – Tight, phase-coherent low frequencies
- **Global Dry/Wet** – Blend processed and DI signal
- **Cab IR Loader** – Load custom impulse responses
- Includes custom "Funky Moose" bass cabinet IRs

---

## 🫎 Interactive Visual Engine
The Moose is not decoration.
It visually reflects signal level, compression intensity, and active features in real time.

- RMS-driven level animation
- Compression glow feedback
- Punch mode visual highlight
- Custom look & feel with dynamic lighting

---

# Presets & MIDI

- Factory presets covering clean, vintage, modern slap, synth-style, and driven tones
- User preset support
- MIDI Learn for fast hardware controller mapping

---

# System & Build

- macOS (Intel & Apple Silicon)
- VST3, AU, Standalone
- Built with JUCE and CMake

Build example:

```bash
cmake -B build
cmake --build build --config Release
```

---

# Status

Funky Moose Amp is actively developed and maintained.
Official signed and notarized builds are provided for distribution releases.

---

If this plugin adds value to your sound, consider supporting the project.
Stay funky.
