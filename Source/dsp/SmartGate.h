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
    noiseFloorLin = 0.0001f; // Approx -80dB
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
    if (!enabled || !isPrepared)
      return;

    auto &block = context.getOutputBlock();
    const int numChannels = (int)block.getNumChannels();
    const int numSamples = (int)block.getNumSamples();

    // 1. Level Detection (Max of channels)
    float maxLevelBlock = 0.0f;

    // To detect level, we filter a temporary sample or utilize the block.
    // For simplicity and speed, we process a single filtered peak for the
    // block.
    for (int i = 0; i < numSamples; ++i) {
      float peakSample = 0.0f;
      for (int ch = 0; ch < numChannels; ++ch) {
        float s = block.getSample(ch, i);
        peakSample = std::max(peakSample, std::abs(s));
      }

      // Filter the peak for detection logic.
      float filtered = sidechainHPF.processSample(peakSample);
      maxLevelBlock = std::max(maxLevelBlock, std::abs(filtered));
    }

    // 2. Dynamic Noise Floor Tracking
    // noiseMarginLin = approx +8dB -> 2.51 gain
    float noiseMarginLin = 2.51f;
    float currentThreshold = noiseFloorLin * noiseMarginLin;

    // If block is very quiet, update noise floor
    // Use a threshold slightly above current noise to allow learning higher
    // noise
    if (maxLevelBlock < noiseFloorLin * 1.5f || maxLevelBlock < 0.0005f) {
      // Very slow learning
      noiseFloorLin = noiseFloorLin * 0.999f + maxLevelBlock * 0.001f;
    }

    // 3. Sustain / Legato Logic
    // If signal stays above threshold, increment counter
    if (maxLevelBlock > currentThreshold * 1.2f) {
      sustainCounter = std::min(sustainCounter + numSamples,
                                (int)(sampleRate * 2.0)); // cap at 2s
    } else {
      // Decay sustain bonus
      sustainCounter = std::max(0, sustainCounter - numSamples * 2);
    }

    // 4. Calculate Target Gain
    float targetGain = 1.0f;

    if (maxLevelBlock < currentThreshold) {
      // Expander Gain
      // Range -35dB -> 0.0177 gain
      float rangeGain = 0.0177f;

      // Sustain Bonus: If we've been playing for a while, don't close as hard
      float sustainMultiplier =
          juce::jlimit(0.0f, 1.0f, (float)sustainCounter / (float)sampleRate);
      float effectiveRange =
          rangeGain + (sustainMultiplier * 0.15f); // Relax up to -16dB

      // Simple expansion ratio logic (linear)
      float ratio = maxLevelBlock / currentThreshold;
      targetGain =
          effectiveRange + (1.0f - effectiveRange) * std::pow(ratio, 2.0f);
    }

    // 5. Apply Smoothing and Process
    gateGain.setTargetValue(targetGain);
    lastGateActivity = 1.0f - targetGain; // 0 = open, 1 = closed

    for (int ch = 0; ch < numChannels; ++ch) {
      auto *data = block.getChannelPointer(ch);
      for (int i = 0; i < numSamples; ++i) {
        data[i] *= gateGain.getNextValue();
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
