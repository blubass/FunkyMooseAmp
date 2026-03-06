# Funky Moose Amp
![Funky Moose Amp](Source/Assets/screenshot_v120.png)

*(Englische Version siehe [README.md](README.md))*

Ein hochgradig vielseitiges, hochmodernes Bassverstärker-Plugin, das speziell dafür entwickelt wurde, den Funk zu bringen! Gebaut mit dem modernen JUCE-Framework und mit einer dynamischen, interaktiven Benutzeroberfläche.

## Funktionsübersicht

### 1. **Tuner & Eingangsbereich**
* **Hochpräziser Tuner:** Immer oben links sichtbar. Reagiert sofort auf dein Eingangssignal.
* **Input-Regler:** Steuert die Menge des Signals, das in den Verstärker (Amp Head) geht.

### 2. **Verstärker & Tone-Modul (Amp & Tone)**
Das Herzstück des Moose. Ein eigens entwickelter Solid-State-Preamp mit optionalen Röhrencharakteristiken.
* **Gain:** Treib die Vorstufe von kristallklar bis hin zu dreckigem Overdrive.
* **Tone Stack (Bass / Mid / Treble):** Sehr reaktionsschnelle, analog modellierte EQs.
* **Volume:** Master-Ausgangslautstärke der Vorstufe.
* **Tube Saturation (Toggle):** Aktiviert eine asymmetrische Sättigungskurve im Röhrenstil, die dynamisch Obertöne hinzufügt.
* **Slap Mode (Toggle):** Ein Mid-Scoop bei 500Hz, gepaart mit einem Bass- & Höhen-Boost (+5dB), speziell abgestimmt für moderne Slap-Bass-Sounds.
* **Low Cut (Toggle):** Ein Hochpassfilter (HPF) bei 40Hz, um das Infraschall-Rumpeln (Subsonic Rumble) aufzuräumen und einen straffen Bassbereich (Low-End) zu erhalten.
* **Auto Gain:** Gleicht die Gesamtlautstärke automatisch an, wenn an den EQ-Reglern gedreht wird.

### 3. **Smart Kompressor (Compressor)**
Ein extrem sauberer, transparenter Kompressor nach VCA-Art.
* **Bedienelemente:** Input (Drive), Threshold, Makeup Gain, Attack, Release.
* **Ratio-Auswahl (Dropdown):** 4:1, 8:1, 12:1 und 20:1 (Limiting).
* **Punch-Schaltung (Toggle):** Hebt die Attack-Phase (Präsenz) hervor und strafft den Bassbereich, wodurch sich Basslinien in jedem Mix durchsetzen.
* **Auto-Makeup (AUTO):** Analysiert die Pegelreduzierung (Gain Reduction) und korrigiert automatisch den Ausgangspegel, um die Lautstärke konstant zu halten.
* **Dry/Wet-Mix:** Eingebaute parallele Kompression ganz ohne Phasenprobleme.
* **Gain-Reduction-Meter:** Visuelles Echtzeit-Feedback, wie stark der Kompressor zupackt.

### 4. **Effekte-Sektion (ModFX)**
Integrierte, hochwertige Effekte, wahlweise seriell oder parallel im Signalweg einschleifbar.
* **Octaver:** Subbass-Generator. Mixt analog klingende "-1 Oktave" und "+1 Oktave" Signale.
* **Envelope Filter (Auto-Wah):** Anschlagdynamischer Filter. Bedienelemente: Attack, Decay, Range.
* **Phaser:** Klassischer optischer 4-Stufen-Phaser mit Reglern für Rate (Geschwindigkeit), Colour (Feedback) und Mix.
* **Chorus:** Ein satter, analog klingender Stereo-Eimerketten-Chorus.
* **Parallel-Modus (Toggle):** Verschiebt den Chorus & Phaser in einen parallelen Signalweg neben dem trockenen/verstärkten Signal, um den maximalen Bass-Punch im Hauptsignal zu erhalten und dem Sound nur Breite (Width) beizumischen.

### 5. **Master-Sektion & Eigene Cab-IRs (Boxensimulation)**
* **Master Out:** Steuert den finalen Output des Plugins.
* **Mono Maker:** Summiert alle tiefen Frequenzen (unterhalb eines bestimmten Grenzwertes) zu Mono. Dies sorgt für absolute Phasen-Kohärenz in den wichtigen, tiefen Registern.
* **Master Dry/Wet:** Mische das gesamte bearbeitete Signal mit deinem cleanen DI-Signal.
* **CAB IR Loader (CAB: OFF/ON):** Lade benutzerdefinierte Impulsantworten (Impulse Responses). Das Plugin enthält bereits maßgeschneiderte "Funky Moose" Bassboxen-Antworten. Einfach umschaltbar und mischbar per "IR Mix".

### 6. **Die interaktive "Funky Moose"-Benutzeroberfläche**
Der Elch (Moose) ist nicht nur ein Logo; er ist eine dynamische Visualisierung deines Tons!
* **Pegel-Visualisierung:** Der Elch reagiert sowohl auf deinen RMS-Eingangs- als auch den Ausgangspegel.
* **Kompressor-Feedback:** Bei starker Pegelreduzierung verändern sich die Hintergrundtextur und das Nachleuchten (Glow).
* **Sonnenbrillen-Glow:** Wenn die "Punch"-Schaltung im Kompressor aktiviert wird, erstrahlt die Sonnenbrille des Elchs in einem grellen Neon-Glühen – und leuchtet bei maximalem Signal satt auf!
* **State-of-the-Art Look & Feel:** Besticht durch skeuomorphische Ästhetik mit realistischen Schatten, leuchtenden Kippschalter-Lampen (State Lights) und einem eigenen Pop-up-System für Presets und Ratios.

## Presets & MIDI
* **Presets:** Enthält Werkspresets ("Factory Bank"), von cleanen DI-Sounds, Vintage Motown, modernem Slap, Synth-Bass bis hin zu starkem Overdrive. Eigenes Speichern in "User"-Bänken wird ebenfalls unterstützt.
* **MIDI Learn:** Einfach auf "MIDI LRN" klicken und direkt danach einen Regler an deinem physischen Controller (z.B. einem Akai MPK Mini) drehen. Schon ist der Parameter mit deinem Regler verknüpft!

## Build-Anweisungen (Für Entwickler)
Kann als VST3, AU und als eigenständige Standalone-App mit CMake gebaut werden.
```bash
cmake -B build
cmake --build build --target FUNKY_MOOSE_AMP_Standalone
```

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
- **Bass / Mid / Treble** – musikalisch abgestimmter, analog-voiced Tone Stack
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
```

---

# Status

Funky Moose Amp wird aktiv weiterentwickelt.
Für offizielle Releases werden signierte und notarisiert geprüfte Builds bereitgestellt.

---

Wenn dieses Plugin deinem Sound echten Mehrwert bringt, unterstütze das Projekt gern.
Stay funky.
