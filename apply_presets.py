import os
import xml.etree.ElementTree as ET

presets_dir = "/Users/uwearthurfelchle/Library/Application Support/FunkyMooseAmp/Presets"
files = [f for f in os.listdir(presets_dir) if f.endswith(".xml")]
files.sort()

def clean_name(name):
    return name.replace(".xml", "")

def escape_name(name):
    return name.replace('"', '\\"')

presets_code = []

presets_code.append("void FunkyMooseAudioProcessor::loadPreset(const juce::String &presetName) {")
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

for filename in files:
    if filename == "Default.xml":
        continue
    filepath = os.path.join(presets_dir, filename)
    name = clean_name(filename)
    name_esc = escape_name(name)
    presets_code.append(f'  if (presetName == "{name_esc}") {{')
    
    tree = ET.parse(filepath)
    root = tree.getroot()
    for param in root.findall('PARAM'):
        id = param.get('id')
        val_str = param.get('value')
        if val_str is not None:
            val = float(val_str)
            presets_code.append(f'    setVal("{id}", {val}f);')
    presets_code.append('    return;')
    presets_code.append('  }')
    presets_code.append('')

for filename in files:
    if filename == "Default.xml":
        filepath = os.path.join(presets_dir, filename)
        name = clean_name(filename)
        name_esc = escape_name(name)
        presets_code.append(f'  if (presetName == "{name_esc}") {{')
        tree = ET.parse(filepath)
        root = tree.getroot()
        for param in root.findall('PARAM'):
            id = param.get('id')
            val_str = param.get('value')
            if val_str is not None:
                val = float(val_str)
                presets_code.append(f'    setVal("{id}", {val}f);')
        presets_code.append('    return;')
        presets_code.append('  }')
        presets_code.append('')

presets_code.append("  auto file = getPresetsFolder().getChildFile(presetName + \".xml\");")
presets_code.append("  if (file.existsAsFile()) {")
presets_code.append("    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);")
presets_code.append("    if (xml != nullptr)")
presets_code.append("      apvts.replaceState(juce::ValueTree::fromXml(*xml));")
presets_code.append("  }")
presets_code.append("}")

presets_code.append("\n\njuce::StringArray FunkyMooseAudioProcessor::getPresetList() {")
presets_code.append("  juce::StringArray list;")
presets_code.append("  list.add(\"F:Default\");")
for filename in files:
    name = clean_name(filename)
    if name != "Default":
        name_esc = escape_name(name)
        presets_code.append(f'  list.add("F:{name_esc}");')

presets_code.append("")
presets_code.append("  auto folder = getPresetsFolder();")
presets_code.append("  juce::Array<juce::File> files;")
presets_code.append("  folder.findChildFiles(files, juce::File::findFiles, false, \"*.xml\");")
presets_code.append("  for (auto &f : files)")
presets_code.append("    list.add(\"U:\" + f.getFileNameWithoutExtension());")
presets_code.append("  return list;")
presets_code.append("}")

presets_text = "\n".join(presets_code)

with open("Source/PluginProcessor.cpp", "r") as f:
    plugin_cpp = f.read()

lines = plugin_cpp.split("\n")
out_lines = []
in_preset_list = False
in_load_preset = False
brace_count = 0

for line in lines:
    if line.startswith("juce::StringArray FunkyMooseAudioProcessor::getPresetList() {") and not in_preset_list and not in_load_preset:
        in_preset_list = True
        brace_count = 1
        continue
    
    if line.startswith("void FunkyMooseAudioProcessor::loadPreset(const juce::String &presetName) {") and not in_preset_list and not in_load_preset:
        in_load_preset = True
        brace_count = 1
        continue
    
    if in_preset_list or in_load_preset:
        if "{" in line:
            brace_count += line.count("{")
        if "}" in line:
            brace_count -= line.count("}")
        
        if brace_count <= 0:
            if in_preset_list:
                in_preset_list = False
            elif in_load_preset:
                in_load_preset = False
        continue
    
    out_lines.append(line)

save_preset_idx = 0
for i, l in enumerate(out_lines):
    if l.startswith("void FunkyMooseAudioProcessor::savePreset(const juce::String &presetName) {"):
        save_preset_idx = i
        break

final_cpp = "\n".join(out_lines[:save_preset_idx]) + presets_text + "\n\n" + "\n".join(out_lines[save_preset_idx:])

with open("Source/PluginProcessor.cpp", "w") as f:
    f.write(final_cpp)
