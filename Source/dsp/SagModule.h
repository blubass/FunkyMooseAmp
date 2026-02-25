#pragma once

#include "../JuceIncludes.h"

//==============================================================================
// Sag Module: Tube Power Supply Collapse Simulation
//
// Mimics the effect of a tube amp's power supply sagging under load.
// Uses a fast Attack and slow Release envelope follower to reduce gain
// when signal peaks occur.
//
class SagModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;
    setAttackMs(5.0f);
    setReleaseMs(100.0f);
    setSagAmount(0.05f);
    prepared = true;
  }

  void setAttackMs(float ms) noexcept {
    ms = juce::jlimit(5.0f, 10.0f, ms);
    attackCoeff = 1.0f - std::exp(-1.0f / (sampleRate * ms / 1000.0f));
  }

  void setReleaseMs(float ms) noexcept {
    ms = juce::jlimit(80.0f, 150.0f, ms);
    releaseCoeff = 1.0f - std::exp(-1.0f / (sampleRate * ms / 1000.0f));
  }

  void setSagAmount(float amount) noexcept {
    sagAmount = juce::jlimit(0.05f, 0.12f, amount);
  }

  void reset() noexcept {
    envelope[0] = 0.0f;
    envelope[1] = 0.0f;
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    auto &block = ctx.getOutputBlock();
    const size_t numCh = block.getNumChannels();

    // Each channel gets its own envelope to avoid stereo-width artefacts
    // when L and R have different peak levels.
    for (size_t ch = 0; ch < numCh; ++ch) {
      auto *d = block.getChannelPointer(ch);
      float &env = envelope[ch < 2 ? ch : 0]; // max 2 channels tracked

      for (size_t i = 0; i < block.getNumSamples(); ++i) {
        float x = d[i];
        float peak = std::abs(x);

        // Attack/Release Envelope Follower (per channel)
        if (peak > env) {
          env += (peak - env) * attackCoeff;
        } else {
          env += (peak - env) * releaseCoeff;
        }

        // Gain Reduction: higher envelope -> more voltage sag
        float gainReduction = 1.0f - (env * sagAmount);
        gainReduction = juce::jlimit(0.0f, 1.0f, gainReduction);

        d[i] = x * gainReduction;
      }
    }
  }

private:
  double sampleRate = 44100.0;
  float attackCoeff = 0.1f;
  float releaseCoeff = 0.01f;
  float sagAmount = 0.05f;
  float envelope[2] = {0.0f, 0.0f}; // Per-channel state (L, R)
  bool prepared = false;
};
