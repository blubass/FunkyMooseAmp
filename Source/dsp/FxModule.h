#pragma once

#include "../JuceIncludes.h"
#include <cmath>
#include <memory>
#include <vector>

// Mojo (post-comp saturation with fixed oversampling)
// v2: Added Asymmetric Saturation for character
class FxModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    mojoOS = std::make_unique<juce::dsp::Oversampling<float>>(
        (int)spec.numChannels,
        2, // 4x oversampling
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    mojoOS->reset();
    mojoOS->initProcessing((size_t)spec.maximumBlockSize);

    mojoDrive01.reset(spec.sampleRate, 0.05);

    juce::dsp::ProcessSpec osSpec = spec;
    osSpec.sampleRate = spec.sampleRate * 4.0;

    for (int i = 0; i < 2; ++i) {
      // 1. Crossover Filters
      lpCrossover[i].prepare(osSpec);
      hpCrossover[i].prepare(osSpec);
      lpCrossover[i].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
      hpCrossover[i].setType(juce::dsp::StateVariableTPTFilterType::highpass);
      lpCrossover[i].setCutoffFrequency(180.0f);
      hpCrossover[i].setCutoffFrequency(180.0f);

      // 2. Pre-Filtering (Meatier clank)
      clankPeak[i].prepare(osSpec);
      clankPeak[i].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
      clankPeak[i].setCutoffFrequency(2400.0f);
      clankPeak[i].setResonance(0.4f);

      // 3. Post-Saturation tame (Cleaner highs)
      fizzTamer[i].prepare(osSpec);
      fizzTamer[i].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
      fizzTamer[i].setCutoffFrequency(3200.0f);
    }

    prepared = true;
  }

  void reset() {
    if (mojoOS)
      mojoOS->reset();
    for (int i = 0; i < 2; ++i) {
      lpCrossover[i].reset();
      hpCrossover[i].reset();
      clankPeak[i].reset();
      fizzTamer[i].reset();
    }
  }

  void setMojoDrive01(float d01) {
    mojoDrive01.setTargetValue(juce::jlimit(0.0f, 1.0f, d01));
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared || mojoOS == nullptr)
      return;

    auto &buffer = ctx.getOutputBlock();
    // Always oversample to avoid latency jumps (prevents clicks)
    juce::dsp::AudioBlock<float> mainBlock = buffer;
    auto osBlock = mojoOS->processSamplesUp(mainBlock);

    const size_t numSamples = osBlock.getNumSamples();
    const size_t numCh = osBlock.getNumChannels();

    for (size_t n = 0; n < numSamples; ++n) {
      const float currentDrive = mojoDrive01.getNextValue();

      if (currentDrive > 0.001f) {
        for (size_t ch = 0; ch < numCh; ++ch) {
          float *x = osBlock.getChannelPointer(ch);
          auto &lpF = lpCrossover[ch < 2 ? ch : 0];
          auto &hpF = hpCrossover[ch < 2 ? ch : 0];
          auto &preF = clankPeak[ch < 2 ? ch : 0];
          auto &postF = fizzTamer[ch < 2 ? ch : 0];

          float sample = x[n];
          if (!std::isfinite(sample))
            sample = 0.0f;

          float low = lpF.processSample(0, sample);
          float high = hpF.processSample(0, sample);

          // Pre-Filter
          high = preF.processSample(0, high);

          // Shaper loop (simplified for stability)
          const float driveExp = 1.0f + (5.0f * currentDrive);
          float hb = high * driveExp;

          const float bias = 0.02f * currentDrive;
          float shaper = std::atan(hb + bias) - std::atan(bias);
          if (!std::isfinite(shaper))
            shaper = 0.0f;

          shaper *= (1.0f + (0.3f * currentDrive));
          high = postF.processSample(0, shaper);

          // Blend with gain compensation (prevent volume jump)
          float out = (low * 1.1f) + (high * 0.55f);
          if (!std::isfinite(out))
            out = 0.0f;
          x[n] = out;
        }
      }
    }

    mojoOS->processSamplesDown(mainBlock);

    // Final safety clip
    for (size_t ch = 0; ch < mainBlock.getNumChannels(); ++ch) {
      float *ptr = mainBlock.getChannelPointer(ch);
      for (size_t i = 0; i < mainBlock.getNumSamples(); ++i) {
        float s = ptr[i];
        if (!std::isfinite(s))
          s = 0.0f;
        ptr[i] = std::tanh(s);
      }
    }
  }

  float getLatencyInSamples() const {
    return mojoOS ? mojoOS->getLatencyInSamples() : 0.0f;
  }

private:
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mojoDrive01{
      0.25f};
  std::unique_ptr<juce::dsp::Oversampling<float>> mojoOS;

  // Filters for Multiband/EQ (per channel)
  juce::dsp::StateVariableTPTFilter<float> lpCrossover[2];
  juce::dsp::StateVariableTPTFilter<float> hpCrossover[2];
  juce::dsp::StateVariableTPTFilter<float> clankPeak[2];
  juce::dsp::StateVariableTPTFilter<float> fizzTamer[2];

  bool prepared{false};
};
