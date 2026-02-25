#pragma once

#include "../JuceIncludes.h"

//==============================================================================
// Simple Cabinet Simulation
// 4x10: Tight, punchy, slight mid-scoop, 5kHz rolloff
// 1x15: Deep, warm, low-mid bump, 3kHz rolloff
//==============================================================================
class CabSim {
public:
  enum Type { Bypass = 0, Cab4x10 = 1, Cab1x15 = 2 };

  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;

    hp.prepare(spec);
    hp.reset();

    lp.prepare(spec);
    lp.reset();

    mid.prepare(spec);
    mid.reset();

    lastType = -1; // Force update
  }

  void reset() {
    hp.reset();
    lp.reset();
    mid.reset();
  }

  void setCabType(int t) noexcept { type = t; }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (type <= Bypass || type > Cab1x15)
      return;

    juce::ScopedNoDenormals noDenormals;

    updateFilters(type);

    hp.process(ctx);
    mid.process(ctx);
    lp.process(ctx);
  }

private:
  void updateFilters(int type) {
    if (type == lastType)
      return;
    lastType = type;

    if (type == Cab4x10) {
      // 4x10: Tight Bass, Punchy Mids
      *hp.state =
          *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 75.0f);
      *lp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate,
                                                                    5500.0f);
      // Slight scoop at 400Hz for "Modern" sound
      *mid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
          sampleRate, 450.0f, 0.8f, 0.7f);
    } else if (type == Cab1x15) {
      // 1x15: Deep Bass, Warm/Dark
      *hp.state =
          *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 45.0f);
      *lp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate,
                                                                    3200.0f);
      // Low-Mid Bump for warmth
      *mid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
          sampleRate, 140.0f, 1.2f, 1.8f); // +5dB approx
    }
  }

  double sampleRate{44100.0};
  int lastType{-1};
  int type = 0;

  // Stereo-ready filters
  using Filter =
      juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                     juce::dsp::IIR::Coefficients<float>>;
  Filter hp, lp, mid;
};
