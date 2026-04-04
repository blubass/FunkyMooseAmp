#pragma once

#include "../JuceIncludes.h"
#include <algorithm>

//==============================================================================
// Sag Module: Tube Power Supply Collapse Simulation
//
// Mimics the effect of a tube amp's power supply sagging under load.
// Uses a fast Attack and slow Release envelope follower to reduce gain
// when signal peaks occur.
//==============================================================================
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
    sagAmount = juce::jlimit(0.0f, 0.5f, amount);
  }

  void reset() noexcept { globalEnv = 0.0f; }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    auto &block = ctx.getOutputBlock();
    const size_t numCh = block.getNumChannels();
    const size_t numSamples = block.getNumSamples();

    // Stereo Linked detection to avoid image shift or per-channel grit
    float blockPeak = 0.0f;
    for (size_t ch = 0; ch < numCh; ++ch) {
      auto *data = block.getChannelPointer(ch);
      float channelPeak = juce::FloatVectorOperations::findMaximum(data, (int)numSamples);
      blockPeak = std::max(blockPeak, channelPeak);
    }

    // Smooth global envelope (RMS-style to prevent audio rate ripple on low frequencies)
    float squaredPeak = blockPeak * blockPeak;
    if (squaredPeak > globalEnv) {
      globalEnv += (squaredPeak - globalEnv) * (float)attackCoeff;
    } else {
      globalEnv += (squaredPeak - globalEnv) * (float)releaseCoeff;
    }

    float gainReduction = 1.0f - (std::sqrt(globalEnv) * sagAmount);
    // Allow more extreme sag (up to 50% reduction) for audibility
    gainReduction = juce::jlimit(0.50f, 1.0f, gainReduction);

    for (size_t ch = 0; ch < numCh; ++ch) {
        if (float* d = block.getChannelPointer(ch))
            juce::FloatVectorOperations::multiply(d, gainReduction, (int)numSamples);
    }
  }

private:
  double sampleRate = 44100.0;
  float attackCoeff = 0.1f;
  float releaseCoeff = 0.01f;
  float sagAmount = 0.05f;
  float globalEnv = 0.0f;
  bool prepared = false;
};
