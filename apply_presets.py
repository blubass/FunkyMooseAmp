import os
import xml.etree.ElementTree as ET

# Pfade definieren
presets_dir = os.path.expanduser("~/Library/Application Support/FunkyMooseAmp/Presets")
processor_path = "Source/PluginProcessor.cpp"

if not os.path.exists(presets_dir):
    print(f"Fehler: {presets_dir} existiert nicht.")
    exit(1)

files = [f for f in os.listdir(presets_dir) if f.endswith(".xml")]
files.sort()

def clean_name(name):
    return name.replace(".xml", "")

def escape_name(name):
    return name.replace('"', '\\"').replace("`", "'")

presets_code = []

# ==============================================================================
# 1. LOAD PRESET FUNCTION
# ==============================================================================
presets_code.append("void FunkyMooseAudioProcessor::loadPreset(const juce::String &presetName) {")
presets_code.append("  currentPresetName = presetName;")
presets_code.append("  for (auto &p : getParameters())")
presets_code.append("    if (auto *rp = dynamic_cast<juce::RangedAudioParameter *>(p))")
presets_code.append("      rp->setValueNotifyingHost(rp->getDefaultValue());")
presets_code.append("")
presets_code.append("  auto setVal = [&](const juce::String &id, float val) {")
presets_code.append("    if (auto *p = apvts.getParameter(id))")
presets_code.append("      p->setValueNotifyingHost(p->convertTo0to1(val));")
presets_code.append("  };")
presets_code.append("  auto setBool = [&](const juce::String &id, bool val) {")
presets_code.append("    if (auto *p = apvts.getParameter(id))")
presets_code.append("      p->setValueNotifyingHost(val ? 1.0f : 0.0f);")
presets_code.append("  };")
presets_code.append("")

# Alphabetisch sortierte Presets generieren
for filename in files:
    filepath = os.path.join(presets_dir, filename)
    name = clean_name(filename)
    name_esc = escape_name(name)
    
    presets_code.append(f'  if (presetName == "{name_esc}") {{')
    
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
        for param in root.findall('PARAM'):
            pid = param.get('id')
            val_str = param.get('value')
            if pid and val_str is not None:
                val = float(val_str)
                presets_code.append(f'    setVal("{pid}", {val}f);')
    except Exception as e:
        print(f"Warnung: {filename} konnte nicht geparst werden: {e}")
        
    presets_code.append('    return;')
    presets_code.append('  }')
    presets_code.append('')

presets_code.append("  // Fallback: Benutzereigene .xml im Preset-Ordner laden")
presets_code.append("  auto file = getPresetsFolder().getChildFile(presetName + \".xml\");")
presets_code.append("  if (file.existsAsFile()) {")
presets_code.append("    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);")
presets_code.append("    if (xml != nullptr)")
presets_code.append("      apvts.replaceState(juce::ValueTree::fromXml(*xml));")
presets_code.append("  }")
presets_code.append("}")

# ==============================================================================
# 2. GET PRESET LIST FUNCTION
# ==============================================================================
presets_code.append("\n\njuce::StringArray FunkyMooseAudioProcessor::getPresetList() {")
presets_code.append("  juce::StringArray list;")
# "F:" steht für "Factory" Presets
for filename in files:
    name = clean_name(filename)
    name_esc = escape_name(name)
    presets_code.append(f'  list.add("F:{name_esc}");')

presets_code.append("")
presets_code.append("  // 'U:' steht für zusätzliche 'User' Presets aus dem Ordner")
presets_code.append("  auto folder = getPresetsFolder();")
presets_code.append("  juce::Array<juce::File> userFiles;")
presets_code.append("  folder.findChildFiles(userFiles, juce::File::findFiles, false, \"*.xml\");")
presets_code.append("  for (auto &f : userFiles) {")
presets_code.append("    juce::String uname = f.getFileNameWithoutExtension();")
presets_code.append("    bool existsInFactory = false;")
for filename in files:
    name_esc = escape_name(clean_name(filename))
    presets_code.append(f'    if (uname == "{name_esc}") existsInFactory = true;')
presets_code.append("    if (!existsInFactory) list.add(\"U:\" + uname);")
presets_code.append("  }")
presets_code.append("  return list;")
presets_code.append("}")

presets_text = "\n".join(presets_code)

# ==============================================================================
# 3. AKTUALISIERUNG VON PLUGINPROCESSOR.CPP
# ==============================================================================
with open(processor_path, "r") as f:
    orig_cpp = f.read()

# Suche nach den Blöcken für loadPreset und getPresetList
new_cpp = re.sub(
    r'void FunkyMooseAudioProcessor::loadPreset\(.*?\)\s*\{.*?\}', 
    'LOAD_PRESET_PLACEHOLDER', 
    orig_cpp, 
    flags=re.DOTALL
)

new_cpp = re.sub(
    r'juce::StringArray FunkyMooseAudioProcessor::getPresetList\(.*?\)\s*\{.*?\}', 
    'GET_PRESET_LIST_PLACEHOLDER', 
    new_cpp, 
    flags=re.DOTALL
)

# Wir teilen die presets_text für die Injektion
load_preset_logic = "\n".join(presets_code[:presets_code.index("\n\njuce::StringArray FunkyMooseAudioProcessor::getPresetList() {")])
get_preset_list_logic = "\n".join(presets_code[presets_code.index("\n\njuce::StringArray FunkyMooseAudioProcessor::getPresetList() {"):])

new_cpp = new_cpp.replace('LOAD_PRESET_PLACEHOLDER', load_preset_logic)
new_cpp = new_cpp.replace('GET_PRESET_LIST_PLACEHOLDER', get_preset_list_logic)

with open(processor_path, "w") as f:
    f.write(new_cpp)

print(f"✅ PluginProcessor.cpp wurde mit {len(files)} Presets aus {presets_dir} aktualisiert.")
