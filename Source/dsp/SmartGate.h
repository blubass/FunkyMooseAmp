#pragma once

#include "../JuceIncludes.h"
#include <algorithm>
#include <cmath>

/**
 * SmartGate: An "Invisible" Noise Gate / Expander with dynamic noise tracking.
 * Features:
 * - Linear noise floor estimation (saves log calls)
 * - 90Hz HPF Sidechain to ignore low-end rumble
 * - Soft expander characteristic
 * - Sustain-Adaptive: relaxes if signal is constant
 * - Stereo-linked detection
 */
class SmartGate {
public:
  SmartGate() {
    // Init smoothing to a neutral state
    gateGain.reset(44100.0, 0.05); // 50ms smoothing
    gateGain.setCurrentAndTargetValue(1.0f);
  }

  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;

    // Setup Sidechain HPF (approx 90-100Hz)
    sidechainHPF.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 95.0f);
    sidechainHPF.prepare(spec);

    gateGain.reset(sampleRate, 0.02); // 20ms smoothing for responsiveness

    // Reset state
    noiseFloorLin = 0.00005f; // Approx -86dB (Lower start)
    sustainCounter = 0;
    isPrepared = true;
  }

  void reset() {
    sidechainHPF.reset();
    gateGain.setCurrentAndTargetValue(1.0f);
    sustainCounter = 0;
  }

  void setEnabled(bool e) { enabled = e; }

  void process(const juce::dsp::ProcessContextReplacing<float> &context) {
    if (!enabled || !isPrepared) {
      gateGain.setCurrentAndTargetValue(1.0f);
      return;
    }

    auto &block = context.getOutputBlock();
    const int numChannels = (int)block.getNumChannels();
    const int numSamples = (int)block.getNumSamples();

    // 1. Unified Level Detection Loop
    float maxLevelBlock = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
      float peakSample = 0.0f;
      for (int ch = 0; ch < numChannels; ++ch) {
        peakSample = std::max(peakSample, std::abs(block.getSample(ch, i)));
      }
      // sidechain HPF processing per loop is okay, but we only need the peak
      float filtered = sidechainHPF.processSample(peakSample);
      float absF = std::abs(filtered);
      if (absF > maxLevelBlock)
        maxLevelBlock = absF;
    }

    // 2. Dynamic Noise Floor Tracking (Faster & More Sensitive)
    float currentThreshold =
        noiseFloorLin * 3.16f; // +10dB margin (instead of 8dB)
    if (maxLevelBlock < noiseFloorLin * 1.3f || maxLevelBlock < 0.0003f) {
      noiseFloorLin = (noiseFloorLin * 0.985f) + (maxLevelBlock * 0.015f);
    }

    // 3. Sustain / Legato Logic
    if (maxLevelBlock > currentThreshold * 1.2f) {
      sustainCounter =
          std::min(sustainCounter + numSamples, (int)(sampleRate * 1.5));
    } else {
      sustainCounter = std::max(0, sustainCounter - numSamples * 3);
    }

    // 4. Calculate Target Gain (Steeper Expansion)
    float targetGain = 1.0f;
    if (maxLevelBlock < currentThreshold) {
      float sustainMultiplier = (float)sustainCounter / (float)sampleRate;
      float effectiveRange =
          0.008f + (sustainMultiplier * 0.12f); // Deeper floor

      float ratio = maxLevelBlock / (currentThreshold + 0.000001f);
      targetGain =
          effectiveRange + (1.0f - effectiveRange) * std::pow(ratio, 2.5f);
    }

    // 5. Apply Gain (Fast Loop)
    gateGain.setTargetValue(targetGain);
    lastGateActivity = 1.0f - targetGain;

    for (int i = 0; i < numSamples; ++i) {
      float g = gateGain.getNextValue();
      for (int ch = 0; ch < numChannels; ++ch) {
        block.getChannelPointer(ch)[i] *= g;
      }
    }
  }

  float getGateActivity() const { return lastGateActivity; }

private:
  bool enabled = true;
  bool isPrepared = false;
  double sampleRate = 44100.0;
  float lastGateActivity = 0.0f;

  juce::dsp::IIR::Filter<float> sidechainHPF;

  juce::SmoothedValue<float> gateGain;
  float noiseFloorLin = 0.0001f;
  int sustainCounter = 0;
};
