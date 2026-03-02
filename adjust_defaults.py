import xml.etree.ElementTree as ET
import re

preset_path = "/Users/uwearthurfelchle/Library/Application Support/FunkyMooseAmp/Presets/Default.xml"
tree = ET.parse(preset_path)
root = tree.getroot()

defaults = {}
for param in root.findall('PARAM'):
    pid = param.get('id')
    pval = param.get('value')
    if pval is not None:
        defaults[pid] = float(pval)

with open("Source/PluginProcessor.cpp", "r") as f:
    lines = f.readlines()

in_create_params = False
for i, line in enumerate(lines):
    if "FunkyMooseAudioProcessor::createParams()" in line:
        in_create_params = True
    if in_create_params and "return" in line and "{" in line and "p.begin()" in line:
        in_create_params = False
        
    if in_create_params:
        # Match make_unique<AP...>("paramName", ...
        # e.g.: p.push_back(std::make_unique<APF>("ampGain", "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), -6.0f));
        m = re.search(r'make_unique<AP([FBC])>\s*\(\s*"([^"]+)"', line)
        if m:
            ptype = m.group(1)
            pid = m.group(2)
            if pid in defaults:
                val = defaults[pid]
                if ptype == 'F':
                    # Need to replace the last float before the closing parenthesis of make_unique
                    # It's usually `... , 0.0f));`
                    # Actually, some span multiple lines. We'll simply find the line that has the closing `));` or `, defaultVal));`
                    # Let's just do a simple replacement if it's on one line for simplicity.
                    pass

# Actually, doing this with regex on C++ can be messy. Let's just create a small C++ tweak inside the constructor:
# We just call loadPreset("Default"); BUT only if this is the first instantiation/no state loaded?
# JUCE automatically calls `setStateInformation` AFTER constructor. So if we just set the apvts values in the constructor, they will be overriden by setStateInformation. Which is perfect!
