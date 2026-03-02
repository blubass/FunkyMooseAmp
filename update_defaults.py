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
        # Match make_unique<.*>("id",
        m = re.search(r'make_unique<AP([FBC])>\s*\(\s*"([^"]+)"', line)
        if m:
            ptype = m.group(1)
            pid = m.group(2)
            if pid in defaults:
                val = defaults[pid]
                if ptype == 'F':
                    # Need to replace the last float before the closing parenthesis of make_unique
                    # Example:  "ampGain", "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), -6.0f));
                    # We can use regex to replace the last float.
                    pass

# Instead of regex the lines, let's just use Python string manipulation.
# For each parameter in defaults, if it's APF, we look for `, (number)f));` and replace it.
# Wait, some lines are split across two lines!
