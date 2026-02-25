Full UI layout patch (design-space 2048x1152, resizable editor 1600x900).
This adds COMP + FX knobs + labels + section boxes so the plugin isn't 'empty'.

Apply:
- overwrite:
  Source/PluginEditor.h
  Source/PluginEditor.cpp

Requires in BinaryData:
- knob_big.png
- knob_small.png

Optional:
- panel_base.png
- panel_wear.png

Then clean build:
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
