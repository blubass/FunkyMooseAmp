# Funky Moose Amp
![Funky Moose Amp](Source/Assets/screenshot_v120.png)

*(Englische Version siehe [README.md](README.md))*

Funky Moose Amp ist ein modernes, professionelles Bassverstärker-Plugin für Klarheit, Punch und Charakter. Entwickelt mit JUCE und optimiert für macOS (Intel & Apple Silicon), kombiniert es eine dynamische Solid-State-Vorstufe, intelligente Dynamikbearbeitung, kreative Modulationseffekte und eine interaktive visuelle Engine.

Kein generischer Amp-Simulator, sondern ein klangformendes Instrument für Funk, Groove und ausdrucksstarkes Bassspiel.

---

# Hauptfunktionen

## 🎚 Input & Tuner
- **Hochpräziser Tuner** – permanent sichtbar und extrem reaktionsschnell  
- **Input Stage** – steuert, wie stark das Signal die Vorstufe ansteuert  

---

## 🔥 Amp & Tone
Eine eigens entwickelte Solid-State-Vorstufe mit optionaler Röhrencharakteristik.

- **Gain** – von kristallklar bis kerniger Overdrive  
- **Bass / Mid / Treble** – musikalisch abgestimmter Tone Stack  
- **Master Volume** – Kontrolle über den Vorstufen-Output  
- **Tube Mode** – asymmetrische harmonische Sättigung für Wärme und Biss  
- **Slap Mode** – moderner Mid-Scoop mit angehobenen Bässen und Höhen  
- **Low Cut** – Hochpass bei 40 Hz für straffes Low-End  
- **Auto Gain Compensation** – hält die wahrgenommene Lautstärke beim Klangformen konstant  

---

## 🎛 Smart Compressor
Transparente VCA-basierte Kompression speziell für Bass.

- **Drive, Threshold, Makeup**  
- **Attack & Release**  
- **Wählbare Ratios** – 4:1, 8:1, 12:1, 20:1  
- **Punch Mode** – betont Transienten und stabilisiert das Low-End  
- **Auto Makeup** – intelligente Pegelkompensation  
- **Dry/Wet Blend** – parallele Kompression ohne Phasenprobleme  
- **Gain-Reduction-Meter** – visuelles Echtzeit-Feedback  

---

## 🎶 Modulation & Effekte (ModFX)
Hochwertige Effekte, seriell oder teilweise parallel nutzbar.

- **Octaver** – Mischung aus -1 und +1 Oktave  
- **Envelope Filter** – anschlagdynamischer Auto-Wah  
- **Phaser** – klassischer 4-Stufen-Modulationssound  
- **Chorus** – analog inspirierte Stereo-Breite  
- **Parallel Mode** – erhält den Punch im Low-End und mischt Bewegung hinzu  

---

## 🧱 Cabinet & Master

- **Master Output**  
- **Mono Maker** – phasenkohärente Tiefen  
- **Global Dry/Wet** – Mischung aus bearbeitetem und DI-Signal  
- **Cab IR Loader** – Laden eigener Impulsantworten  
- Inklusive maßgeschneiderter „Funky Moose“ Bass-Cab-IRs  

---

## 🫎 Interaktive Visual Engine
Der Moose ist mehr als ein Logo.  
Er reagiert in Echtzeit auf Pegel, Kompression und aktive Funktionen.

- RMS-basierte Level-Animation  
- Kompressions-Glow  
- Visuelles Feedback im Punch Mode  
- Eigenes dynamisches Look & Feel mit subtiler Lichtästhetik  

---

# Presets & MIDI

- Werkspresets für Clean, Vintage, Modern Slap, Synth-ähnliche Sounds und Drive  
- Benutzer-Presets speicherbar  
- MIDI Learn zur schnellen Controller-Zuweisung  

---

# System & Build

- macOS (Intel & Apple Silicon)  
- VST3, AU, Standalone  
- Entwickelt mit JUCE und CMake  

Beispiel-Build:

```bash
cmake -B build
cmake --build build --config Release
