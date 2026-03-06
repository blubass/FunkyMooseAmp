# Funky Moose Amp
![Funky Moose Amp](Source/Assets/screenshot_v120.png)

*(German version see [README_de.md](README_de.md))*

Funky Moose Amp is a modern, professional bass amplifier plugin designed for clarity, punch, and character. Built with JUCE and engineered for macOS (Intel & Apple Silicon), it combines a responsive solid-state preamp, intelligent dynamics, creative modulation effects, and a distinctive interactive interface.

This is not a generic amp simulator. It is a tone-shaping instrument built for funk, groove, and expressive bass playing.

---

# Key Features

## 🎚 Input & Tuner
- **High-Precision Tuner** – always visible and instantly responsive  
- **Input Stage** – controls how hard the signal drives the preamp  

---

## 🔥 Amp & Tone
A custom-designed solid-state preamp with optional tube-style saturation.

- **Gain** – from clean articulation to gritty drive  
- **Bass / Mid / Treble** – musical tone stack  
- **Master Volume** – preamp output control  
- **Tube Mode** – asymmetric harmonic saturation for warmth and edge  
- **Slap Mode** – modern mid-scoop voicing with enhanced lows and highs  
- **Low Cut** – 40 Hz high-pass filter for tighter low-end  
- **Auto Gain Compensation** – maintains consistent perceived loudness  

---

## 🎛 Smart Compressor
Transparent VCA-style compression designed specifically for bass.

- **Drive, Threshold, Makeup**  
- **Attack & Release**  
- **Selectable Ratios** – 4:1, 8:1, 12:1, 20:1  
- **Punch Mode** – enhanced transient response and low-end control  
- **Auto Makeup** – intelligent level compensation  
- **Dry/Wet Blend** – parallel compression without phase artifacts  
- **Gain Reduction Meter** – real-time visual feedback  

---

## 🎶 Modulation & Effects (ModFX)
High-quality built-in effects, usable in serial or partial parallel routing.

- **Octaver** – blend of -1 and +1 octave  
- **Envelope Filter** – touch-sensitive auto-wah  
- **Phaser** – classic 4-stage modulation  
- **Chorus** – analog-inspired stereo width  
- **Parallel Mode** – preserve low-end clarity while adding movement  

---

## 🧱 Cabinet & Master

- **Master Output**  
- **Mono Maker** – phase-coherent low frequencies  
- **Global Dry/Wet** – blend processed and DI signal  
- **Cab IR Loader** – load custom impulse responses  
- Includes custom “Funky Moose” bass cabinet IRs  

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
