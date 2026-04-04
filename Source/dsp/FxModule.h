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

    lastLoFreq = 180.0f;
    lastClankFreq = 2400.0f;
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

  void setCrossoverFreq(float freq) {
    targetLoFreq = juce::jlimit(80.0f, 400.0f, freq);
  }

  void setClankFreq(float freq) {
    targetClankFreq = juce::jlimit(800.0f, 4000.0f, freq);
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

    // 1. Dynamic tuning update (only if changed significantly)
    if (std::abs(targetLoFreq - lastLoFreq) > 1.0f || std::abs(targetClankFreq - lastClankFreq) > 1.0f) {
      for (int i = 0; i < 2; ++i) {
        lpCrossover[i].setCutoffFrequency(targetLoFreq);
        hpCrossover[i].setCutoffFrequency(targetLoFreq);
        clankPeak[i].setCutoffFrequency(targetClankFreq);
      }
      lastLoFreq = targetLoFreq;
      lastClankFreq = targetClankFreq;
    }

    auto fastAtan = [](float x) noexcept {
      // Rational approximation for atan(x)
      float x2 = x * x;
      return x * (1.0f + 0.28086f * x2) / (1.0f + 0.614447f * x2 + 0.03029f * x2 * x2);
    };

    // Pre-calculate drive-independent sample buffers
    float* chPtrs[2] = { nullptr, nullptr };
    for (size_t ch = 0; ch < std::min(numCh, (size_t)2); ++ch)
        chPtrs[ch] = osBlock.getChannelPointer(ch);

    for (size_t n = 0; n < numSamples; ++n) {
      const float currentDrive = mojoDrive01.getNextValue();

      if (currentDrive > 0.001f) {
        const float driveExp = 1.0f + (5.0f * currentDrive);
        const float bias = 0.02f * currentDrive;
        const float atanBias = fastAtan(bias); // Optimized: Use fastAtan and moved out of inner if possible
        const float outGainComp = (1.0f + (0.3f * currentDrive));

        for (size_t ch = 0; ch < std::min(numCh, (size_t)2); ++ch) {
          float *x = chPtrs[ch];
          auto &lpF = lpCrossover[ch];
          auto &hpF = hpCrossover[ch];
          auto &preF = clankPeak[ch];
          auto &postF = fizzTamer[ch];

          float sample = x[n];
          if (!std::isfinite(sample)) sample = 0.0f;

          float low = lpF.processSample(0, sample);
          float high = hpF.processSample(0, sample);

          // Pre-Filter
          high = preF.processSample(0, high);

          // Shaper loop (Asymmetric Saturation) using fast atan
          float hb = high * driveExp;
          float shaper = fastAtan(hb + bias) - atanBias;
          
          if (!std::isfinite(shaper)) shaper = 0.0f;

          shaper *= outGainComp;
          high = postF.processSample(0, shaper);

          // Blend with gain compensation (prevent volume jump)
          float out = (low * 1.1f) + (high * 0.55f);
          if (!std::isfinite(out)) out = 0.0f;
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
        
        // Fast tanh approximation for safety clip
        if (s <= -3.0f) ptr[i] = -1.0f;
        else if (s >= 3.0f) ptr[i] = 1.0f;
        else ptr[i] = s * (27.0f + s * s) / (27.0f + 9.0f * s * s);
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

  float targetLoFreq = 180.0f;
  float lastLoFreq = 180.0f;
  float targetClankFreq = 2400.0f;
  float lastClankFreq = 2400.0f;

  bool prepared{false};
};
