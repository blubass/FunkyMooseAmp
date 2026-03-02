#pragma once

#include "../JuceIncludes.h"
#include "PitchShifter.h"
#include <cmath>
#include <vector>

//==============================================================================
/**
    Octaver + Envelope Filter module.
    Clean Digital Pitch Shifting (Modern Algorithm).
*/
//==============================================================================
class OctEnvModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = (float)spec.sampleRate;
    maxBlockSize = (int)spec.maximumBlockSize;
    numChannels = (int)spec.numChannels;

    shifter.prepare(sampleRate);
    shifterUp.prepare(sampleRate);

    envFilter.reset();
    envFilter.prepare(spec);
    envFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    oct1Sm.reset(sampleRate, 0.05);
    oct2Sm.reset(sampleRate, 0.05);
    octMixSm.reset(sampleRate, 0.05);

    preShifterHPF.prepare(spec);
    preShifterHPF.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 60.0f);

    reset();

    // Ensure scratch buffers are ready
    dryScratch.assign((size_t)maxBlockSize, 0.0f);
    subScratch.assign((size_t)maxBlockSize, 0.0f);
    upScratch.assign((size_t)maxBlockSize, 0.0f);

    prepared = true;

    dryDelayLine.assign((size_t)(delayLineSize * numChannels), 0.0f);
    delayPtr = 0;
  }

  void reset() {
    octEnv = 0.0f;
    envFollower = 0.0f;
    envFilter.reset();
    shifter.reset();
    shifterUp.reset();
    preShifterHPF.reset();
    std::fill(dryDelayLine.begin(), dryDelayLine.end(), 0.0f);
    delayPtr = 0;
  }

  void setOctaveOn(bool on) noexcept { octOn = on; }
  void setOctave1(float v) noexcept { oct1Sm.setTargetValue(v); }
  void setOctave2(float v) noexcept { oct2Sm.setTargetValue(v); }
  void setOctaveMix(float v) noexcept { octMixSm.setTargetValue(v); }

  void setEnvelopeOn(bool on) noexcept { envOn = on; }
  void setEnvAttack(float v) noexcept { envAttack01 = v; }
  void setEnvDecay(float v) noexcept { envDecay01 = v; }
  void setEnvRange(float v) noexcept { envRange01 = v; }

  int getLatencyInSamples() const { return shifter.getLatency(); }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    juce::dsp::AudioBlock<float> &buffer = ctx.getOutputBlock();
    const int nSamp = (int)buffer.getNumSamples();
    if (nSamp <= 0)
      return;

    juce::ScopedNoDenormals noDenormals;

    const int chs =
        juce::jmin((int)buffer.getNumChannels(), juce::jmax(1, numChannels));

    // ==============================================================================
    // 1. OCTAVER (Modern Pitch Shifting)
    // ==============================================================================
    if (octOn) {
      ensureScratch(nSamp);

      // Copy input to scratch for PitchShifter (mono sum for better tracking)
      auto *inL = buffer.getChannelPointer(0);
      if (chs > 1) {
        auto *inR = buffer.getChannelPointer(1);
        for (int i = 0; i < nSamp; ++i)
          dryScratch[(size_t)i] = (inL[i] + inR[i]) * 0.5f;
      } else {
        for (int i = 0; i < nSamp; ++i)
          dryScratch[(size_t)i] = inL[i];
      }

      // HPF @ 60Hz to stabilize STFT tracking (Rumble removal)
      for (int i = 0; i < nSamp; ++i)
        dryScratch[(size_t)i] =
            preShifterHPF.processSample(dryScratch[(size_t)i]);

      // Generate Sub Octave (Check target value for efficiency gating)
      if (oct1Sm.getTargetValue() > 0.001f)
        shifter.process(0.5f, nSamp, dryScratch.data(), subScratch.data());
      else
        std::fill(subScratch.begin(), subScratch.begin() + nSamp, 0.0f);

      // Generate Up Octave (Check target value for efficiency gating)
      if (oct2Sm.getTargetValue() > 0.001f)
        shifterUp.process(2.0f, nSamp, dryScratch.data(), upScratch.data());
      else
        std::fill(upScratch.begin(), upScratch.begin() + nSamp, 0.0f);

      // Timing constants for mix logic
      const float mixK = 1.0f;
      const float dryGain = juce::Decibels::decibelsToGain(0.0f);

      for (int i = 0; i < nSamp; ++i) {
        float sOct1 = oct1Sm.getNextValue();
        float sOct2 = oct2Sm.getNextValue();
        float sMix = octMixSm.getNextValue();

        float downVal = subScratch[(size_t)i] * sOct1;
        float upVal = upScratch[(size_t)i] * sOct2;
        float wetSignal = (downVal + upVal);

        // Mix weights: Perfect linear crossfade
        float dryWeight = 1.0f - sMix;
        float wetWeight =
            sMix * 1.1f; // Slight boost to octaves to feel 'present'

        for (int ch = 0; ch < chs; ++ch) {
          // LATENCY COMPENSATION: Delay the dry signal to match shifter
          float dryRaw = buffer.getSample(ch, i);

          // Store in circular buffer for latency alignment
          dryDelayLine[(size_t)(delayPtr * numChannels + ch)] = dryRaw;
        }

        // Read delayed dry signal
        int readPtr =
            (delayPtr - getLatencyInSamples() + delayLineSize) % delayLineSize;

        for (int ch = 0; ch < chs; ++ch) {
          float dryDelayed = dryDelayLine[(size_t)(readPtr * numChannels + ch)];
          float wet = wetSignal * wetWeight;

          // Combine and apply a very soft safety clip to the combined result
          float mixed = (dryDelayed * dryWeight) + wet;
          buffer.setSample(ch, i, softClipTanh(mixed, 1.5f));
        }

        delayPtr = (delayPtr + 1) % delayLineSize;
      }
    } else {
      oct1Sm.skip(nSamp);
      oct2Sm.skip(nSamp);
      octMixSm.skip(nSamp);
    }

    // ==============================================================================
    // 2. ENVELOPE FILTER
    // ==============================================================================
    if (envOn) {
      const float attackCoef =
          std::exp(-1.0f / (sampleRate * (0.005f + envAttack01 * 0.05f)));
      const float releaseCoef =
          std::exp(-1.0f / (sampleRate * (0.05f + envDecay01 * 0.5f)));

      for (int i = 0; i < nSamp; ++i) {
        // STEREO LINKED DETECTION (Used to be left-only)
        float inAbsL = std::abs(buffer.getSample(0, i));
        float inAbsR = (chs > 1) ? std::abs(buffer.getSample(1, i)) : 0.0f;
        const float inAbs = juce::jmax(inAbsL, inAbsR);

        if (inAbs > envFollower)
          envFollower = inAbs + attackCoef * (envFollower - inAbs);
        else
          envFollower = inAbs + releaseCoef * (envFollower - inAbs);

        float cutoff = 150.0f + 5500.0f * envRange01 * envFollower;
        cutoff = juce::jlimit(40.0f, 12000.0f, cutoff);

        envFilter.setCutoffFrequency(cutoff);
        envFilter.setResonance(juce::jmap(envRange01, 0.0f, 1.0f, 2.0f, 8.0f));

        for (int ch = 0; ch < chs; ++ch) {
          float s = buffer.getSample(ch, i);
          s = envFilter.processSample(ch, s);
          buffer.setSample(ch, i, s);
        }
      }
    }
  }

private:
  static inline float softClipTanh(float x, float k) noexcept {
    const float a = 0.92f; // Pre-calculated approx for tanh(1.5)
    return std::tanh(k * x) / a;
  }

  void ensureScratch(int needed) {
    if (needed <= maxBlockSize)
      return;
    dryScratch.assign((size_t)needed, 0.0f);
    subScratch.assign((size_t)needed, 0.0f);
    upScratch.assign((size_t)needed, 0.0f);
    maxBlockSize = needed;
  }

  float sampleRate{44100.0f};
  int maxBlockSize{512};
  int numChannels{2};
  bool prepared{false};

  PitchShifter shifter;
  PitchShifter shifterUp;
  std::vector<float> dryScratch;
  std::vector<float> subScratch;
  std::vector<float> upScratch;

  // Parameters
  bool octOn = false;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oct1Sm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oct2Sm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> octMixSm{0.0f};

  bool envOn = false;
  float envAttack01 = 0.0f;
  float envDecay01 = 0.0f;
  float envRange01 = 0.0f;

  // Octaver state
  float octEnv{0.0f};

  // Envelope State
  juce::dsp::StateVariableTPTFilter<float> envFilter;
  float envFollower{0.0f};

  // Pre-Shifter HPF for tracking stability
  juce::dsp::IIR::Filter<float> preShifterHPF;

  // Latency compensation for Dry Signal
  static const int delayLineSize = 4096;
  std::vector<float> dryDelayLine;
  int delayPtr = 0;
};
