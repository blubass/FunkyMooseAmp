#pragma once

#include "../JuceIncludes.h"

//==============================================================================
// Simple Cabinet Simulation
// 4x10: Tight, punchy, slight mid-scoop, 5kHz rolloff
// 1x15: Deep, warm, low-mid bump, 3kHz rolloff
//==============================================================================
class CabSim {
public:
  enum Type { Bypass = 0, Cab4x10 = 1, Cab1x15 = 2, CustomIR = 3 };

  juce::dsp::Convolution convolver;
  juce::dsp::DryWetMixer<float> mixer;
  juce::String customIrPath;
  bool isCustomIrLoaded = false;

  void loadCustomIr(const juce::String &path) {
    juce::File file(path);
    if (file.existsAsFile()) {
      customIrPath = path;
      convolver.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::yes, 0,
                                    juce::dsp::Convolution::Normalise::yes);
      isCustomIrLoaded = true;
    }
  }

  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;

    hp.prepare(spec);
    hp.reset();

    lp.prepare(spec);
    lp.reset();

    mid.prepare(spec);
    mid.reset();

    convolver.prepare(spec);

    mixer.prepare(spec);
    mixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);
    mixer.setWetLatency(convolver.getLatency());

    lastType = -1; // Force update
  }

  void reset() {
    hp.reset();
    lp.reset();
    mid.reset();
    convolver.reset();
    mixer.reset();
  }

  void setCabType(int t) noexcept { type = t; }
  void setMix(float m) noexcept { mixer.setWetMixProportion(m); }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (type <= Bypass || type > CustomIR)
      return;

    mixer.setWetLatency(convolver.getLatency());
    mixer.pushDrySamples(ctx.getInputBlock());

    juce::ScopedNoDenormals noDenormals;

    if (type == CustomIR) {
      if (isCustomIrLoaded) {
        convolver.process(ctx);
        // Boost IR signal: standard normalization often leaves IRs at -12dB or
        // lower. We boost by 6dB to bring it closer to the dry signal level.
        ctx.getOutputBlock().multiplyBy(juce::Decibels::decibelsToGain(6.0f));
      }
    } else {
      updateFilters(type);
      hp.process(ctx);
      mid.process(ctx);
      lp.process(ctx);
    }

    mixer.mixWetSamples(ctx.getOutputBlock());
  }

private:
  void updateFilters(int type) {
    if (type == lastType)
      return;
    lastType = type;

    if (type == Cab4x10) {
      // 4x10: Deep Mid Scoop for 'Modern' clarity
      *hp.state =
          *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 65.0f);
      *lp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate,
                                                                    6500.0f);
      // Deeper scoop at 400Hz to remove 'mittig' boxiness
      *mid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
          sampleRate, 400.0f, 0.7f, 0.5f); // Deeper scoop (-6dB)
    } else if (type == Cab1x15) {
      // 1x15: Solid Bass, controlled Low-Mids
      *hp.state =
          *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 40.0f);
      *lp.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate,
                                                                    4200.0f);
      // Reduced Low-Mid Bump to avoid muddiness
      *mid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
          sampleRate, 160.0f, 1.0f, 1.4f); // +3dB approx
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
