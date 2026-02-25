#pragma once
#include "../JuceIncludes.h"
#include <cmath>

struct TunerNote {
  juce::String name;
  int octave = 0;
  int cents = 0; // -50..+50
  bool valid = false;
};

inline TunerNote freqToNote(float f) {
  TunerNote tn{};
  if (f <= 0.0f)
    return tn;

  const double a4 = 440.0;
  const double midi = 69.0 + 12.0 * std::log2(f / a4);
  const int midiRound = (int)std::round(midi);

  static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  tn.name = names[(midiRound % 12 + 12) % 12];
  tn.octave = (midiRound / 12) - 1;

  const double fRound = a4 * std::pow(2.0, (midiRound - 69) / 12.0);
  const double cents = 1200.0 * std::log2(f / fRound);
  tn.cents = (int)juce::jlimit(-50.0, 50.0, cents);

  tn.valid = true;
  return tn;
}
