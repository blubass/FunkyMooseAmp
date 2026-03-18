import xml.etree.ElementTree as ET
import re
import os

# Pfade definieren
# Die Tilde (~) wird automatisch zum Pfad des Benutzers erweitert
preset_path = os.path.expanduser("~/Library/Application Support/FunkyMooseAmp/Presets/Default.xml")
processor_path = "Source/PluginProcessor.cpp"

if not os.path.exists(preset_path):
    print(f"Fehler: {preset_path} nicht gefunden.")
    print("Tipp: Starte das Plugin, stelle deine Standard-Einstellungen ein und speichere ein Preset namens 'Default'.")
    exit(1)

# XML laden
try:
    tree = ET.parse(preset_path)
    root = tree.getroot()
except Exception as e:
    print(f"Fehler beim Parsen der XML: {e}")
    exit(1)

# Alle Parameter-IDs und Werte extrahieren
defaults = {}
for param in root.findall('PARAM'):
    pid = param.get('id')
    pval = param.get('value')
    if pid and pval is not None:
        defaults[pid] = pval

# PluginProcessor.cpp lesen
with open(processor_path, "r") as f:
    lines = f.readlines()

# Regex für Parameter-Definitionen in createParams()
# Erkennt make_unique<APF>, <APB> oder <APC> und findet ID sowie den letzten Wert vor ));
# Beispiel: make_unique<APF>("ampGain", "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), -6.0f));
# Wir unterstützen auch Zeilenumbrüche innerhalb der Definition.
param_regex = re.compile(r'make_unique<AP([FBC])>\s*\(\s*"([^"]+)"')

new_lines = []
in_create_params = False
modified_count = 0

for i, line in enumerate(lines):
    if "FunkyMooseAudioProcessor::createParams()" in line:
        in_create_params = True
    
    if in_create_params and "return {p.begin(), p.end()};" in line:
        in_create_params = False

    if in_create_params:
        # Suche nach Parameter-ID
        match = param_regex.search(line)
        if match:
            ptype = match.group(1) # F, B oder C
            pid = match.group(2)
            
            if pid in defaults:
                val = defaults[pid]
                
                # Den Default-Wert in der Zeile (oder Folgelinien) finden
                # Er ist immer das letzte Argument vor ));
                target_line_idx = i
                while "));" not in lines[target_line_idx] and target_line_idx < i + 3:
                    target_line_idx += 1
                
                current_target_line = lines[target_line_idx]
                
                # Finde den Wert zwischen dem letzten Komma und ));
                # Regex sucht rückwärts vom Ende der Zeile
                val_match = re.search(r',\s*([^,)]+)\s*\)\);', current_target_line)
                if val_match:
                    old_val = val_match.group(1).strip()
                    
                    # Neuen Wert formatieren
                    new_val_str = ""
                    if ptype == 'F':
                        new_val_str = f"{float(val):.2f}f"
                    elif ptype == 'B':
                        new_val_str = "true" if float(val) > 0.5 else "false"
                    elif ptype == 'C':
                        new_val_str = str(int(float(val)))
                        
                    if old_val != new_val_str:
                        lines[target_line_idx] = current_target_line.replace(old_val, new_val_str)
                        modified_count += 1
                        print(f"  Fixed {pid}: {old_val} -> {new_val_str}")
    
    new_lines.append(lines[i])

if modified_count > 0:
    with open(processor_path, "w") as f:
        f.writelines(lines)
    print(f"✅ Fertig! {modified_count} Standardwerte wurden in {processor_path} aktualisiert.")
else:
    print("ℹ️ Keine Änderungen nötig. Alle Standardwerte im Code entsprechen der Default.xml.")
