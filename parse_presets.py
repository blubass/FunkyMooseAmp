import os
import xml.etree.ElementTree as ET

presets_dir = "/Users/uwearthurfelchle/Library/Application Support/FunkyMooseAmp/Presets"
files = [f for f in os.listdir(presets_dir) if f.endswith(".xml")]
files.sort()

def clean_name(name):
    return name.replace(".xml", "")

print("void FunkyMooseAudioProcessor::loadPreset(const juce::String &presetName) {")
print("  for (auto &p : getParameters())")
print("    if (auto *rp = dynamic_cast<juce::RangedAudioParameter *>(p))")
print("      rp->setValueNotifyingHost(rp->getDefaultValue());")
print("")
print("  auto setVal = [&](const juce::String &id, float val) {")
print("    if (auto *p = apvts.getParameter(id))")
print("      p->setValueNotifyingHost(p->convertTo0to1(val));")
print("  };")
print("  auto setBool = [&](const juce::String &id, bool val) {")
print("    if (auto *p = apvts.getParameter(id))")
print("      p->setValueNotifyingHost(val ? 1.0f : 0.0f);")
print("  };")
print("")

for filename in files:
    if filename == "Default.xml":
        continue
    filepath = os.path.join(presets_dir, filename)
    name = clean_name(filename)
    print(f'  if (presetName == "{name}") {{')
    
    tree = ET.parse(filepath)
    root = tree.getroot()
    for param in root.findall('PARAM'):
        id = param.get('id')
        val_str = param.get('value')
        if val_str is not None:
            val = float(val_str)
            print(f'    setVal("{id}", {val}f);')
    print('    return;')
    print('  }')
    print('')

for filename in files:
    if filename == "Default.xml":
        filepath = os.path.join(presets_dir, filename)
        name = clean_name(filename)
        print(f'  if (presetName == "{name}") {{')
        tree = ET.parse(filepath)
        root = tree.getroot()
        for param in root.findall('PARAM'):
            id = param.get('id')
            val_str = param.get('value')
            if val_str is not None:
                val = float(val_str)
                print(f'    setVal("{id}", {val}f);')
        print('    return;')
        print('  }')
        print('')

print("  auto file = getPresetsFolder().getChildFile(presetName + \".xml\");")
print("  if (file.existsAsFile()) {")
print("    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);")
print("    if (xml != nullptr)")
print("      apvts.replaceState(juce::ValueTree::fromXml(*xml));")
print("  }")
print("}")

print("\n\njuce::StringArray FunkyMooseAudioProcessor::getPresetList() {")
print("  juce::StringArray list;")
print("  list.add(\"F:Default\");")
for filename in files:
    name = clean_name(filename)
    if name != "Default":
        name_esc = name.replace('"', '\\"')
        print(f'  list.add("F:{name_esc}");')

print("")
print("  auto folder = getPresetsFolder();")
print("  juce::Array<juce::File> files;")
print("  folder.findChildFiles(files, juce::File::findFiles, false, \"*.xml\");")
print("  for (auto &f : files)")
print("    list.add(\"U:\" + f.getFileNameWithoutExtension());")
print("  return list;")
print("}")
