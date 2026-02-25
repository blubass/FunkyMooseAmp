#pragma once

#include "../JuceIncludes.h"
#include "PitchShifter.h"
#include <cmath>
#include <vector>

//==============================================================================
// Octaver + Envelope Filter module.
// - Modern Mode: Clean Digital Pitch Shifting (Oct 1) + Clean Synth (Oct 2)
// - Vintage Mode: Analog Style Synthesis (Oct 1 Sub Sine + Oct 2 Squaring)
//==============================================================================
class OctEnvModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = (float)spec.sampleRate;
    maxBlockSize = (int)spec.maximumBlockSize;
    numChannels = (int)spec.numChannels;

    shifter.prepare(sampleRate);

    envFilter.reset();
    envFilter.prepare(spec);
    envFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    reset();

    // Ensure scratch buffers are ready
    dryScratch.assign((size_t)maxBlockSize, 0.0f);
    subScratch.assign((size_t)maxBlockSize, 0.0f);

    prepared = true;
  }

  void reset() {
    octInputFilter = 0.0f;
    octLowFilter = 0.0f;
    octIntegrator = 0.0f;
    octEnv = 0.0f;
    octFlip = false;
    octLastAbove = false;
    octHighFilter = 0.0f;
    octHighSmooth = 0.0f;

    envFollower = 0.0f;
    envFilter.reset();
  }

  void setOctaveOn(bool on) noexcept { octOn = on; }
  void setOctave1(float v) noexcept { oct1Sm.setTargetValue(v); }
  void setOctave2(float v) noexcept { oct2Sm.setTargetValue(v); }
  void setOctaveMix(float v) noexcept { octMixSm.setTargetValue(v); }
  void setModernMode(bool modern) noexcept { modernMode = modern; }

  void setEnvelopeOn(bool on) noexcept { envOn = on; }
  void setAvailable(bool v) noexcept { /* placeholder */ } // ?

  void setEnvAttack(float v) noexcept { envAttack01 = v; }
  void setEnvDecay(float v) noexcept { envDecay01 = v; }
  void setEnvRange(float v) noexcept { envRange01 = v; }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    auto &buffer = ctx.getOutputBlock();
    const int nSamp = (int)buffer.getNumSamples();
    if (nSamp <= 0)
      return;

    const int chs =
        juce::jmin((int)buffer.getNumChannels(), juce::jmax(1, numChannels));

    // ==============================================================================
    // 1. OCTAVER
    // ==============================================================================
    if (octOn) {
      if (modernMode) {
        // --- MODERN MODE (Digital Clean) ---
        ensureScratch(nSamp);

        // Copy input to scratch for PitchShifter (mono sum or ch 0)
        auto *inPtr = buffer.getChannelPointer(0);
        for (int i = 0; i < nSamp; ++i)
          dryScratch[(size_t)i] = inPtr[i];

        // Generate Sub Octave (PitchShifter)
        shifter.process(0.5f, nSamp, dryScratch.data(), subScratch.data());

        // Envelope follower for dynamics (shared)
        const float envAtkMod = std::exp(-1.0f / (sampleRate * 0.005f));
        const float envRelMod = std::exp(-1.0f / (sampleRate * 0.04f));

        for (int i = 0; i < nSamp; ++i) {
          const float in = dryScratch[(size_t)i];
          const float absIn = std::abs(in);

          // Track Envelope
          if (absIn > octEnv)
            octEnv = absIn + envAtkMod * (octEnv - absIn);
          else
            octEnv = absIn + envRelMod * (octEnv - absIn);

          // Modern High Octave (Clean Squaring - borrowed from Vintage for
          // consistency)
          float sq = in * in;
          octHighFilter += (sq - octHighFilter) * 0.1f;
          float highOct = (sq - octHighFilter);
          highOct = highOct / (octEnv + 0.01f); // Normalize
          octHighSmooth += (highOct - octHighSmooth) * 0.4f;

          // Mix Modern
          // Oct 1 = PitchShifter Result
          // Oct 2 = Clean Synth
          float sOct1 = oct1Sm.getNextValue();
          float sOct2 = oct2Sm.getNextValue();
          float sMix = octMixSm.getNextValue();

          float wetSignal = (subScratch[(size_t)i] * sOct1 * 1.0f) +
                            (octHighSmooth * 6.0f * octEnv * 1.5f * sOct2);

          // Clip
          wetSignal = std::tanh(wetSignal);

          for (int ch = 0; ch < chs; ++ch) {
            float dry = buffer.getSample(ch, i);
            buffer.setSample(ch, i, dry * (1.0f - sMix) + wetSignal * sMix);
          }
        }
      } else {
        // --- VINTAGE MODE (Analog Style) ---
        // Coefficients
        const float envAtk = std::exp(-1.0f / (sampleRate * 0.005f));
        const float envRel =
            std::exp(-1.0f / (sampleRate * 2.000f)); // Massive Sustain
        const float slewRate = 0.05f; // Adjusted for better level
        const float trackThresh = 0.003f;
        const float gateThresh = 0.005f;

        for (int i = 0; i < nSamp; ++i) {
          const float in = buffer.getSample(0, i);
          const float absIn = std::abs(in);

          // Process Envelope
          if (absIn > octEnv)
            octEnv = absIn + envAtk * (octEnv - absIn);
          else
            octEnv = absIn + envRel * (octEnv - absIn);

          // Track Pitch
          octInputFilter += (in - octInputFilter) * 0.04f;
          octLowFilter += (octInputFilter - octLowFilter) * 0.04f;

          if (octLowFilter > trackThresh) {
            if (!octLastAbove) {
              octFlip = !octFlip;
              octLastAbove = true;
            }
          } else if (octLowFilter < -trackThresh) {
            octLastAbove = false;
          }

          // Generate Oct 1 (Sine)
          float target = octFlip ? 1.0f : -1.0f;
          if (octIntegrator < target)
            octIntegrator = std::min(target, octIntegrator + slewRate);
          else if (octIntegrator > target)
            octIntegrator = std::max(target, octIntegrator - slewRate);

          float lowOct =
              octIntegrator * (1.5f - 0.5f * octIntegrator * octIntegrator);
          float gate = (octEnv < gateThresh)
                           ? std::max(0.0f, octEnv / gateThresh)
                           : 1.0f;
          lowOct *= (octEnv * gate);

          // Generate Oct 2 (Squaring)
          float sq = in * in;
          octHighFilter += (sq - octHighFilter) * 0.1f;
          float highOct = (sq - octHighFilter);
          highOct = highOct / (octEnv + 0.01f);
          octHighSmooth += (highOct - octHighSmooth) * 0.4f;
          highOct = octHighSmooth * 8.0f * (octEnv * 1.5f);

          // Mix Vintage
          float sOct1 =
              oct1Sm
                  .getNextValue(); // Careful: called in Modern too.
                                   // But Modern is in 'if (modernMode)' block.
                                   // However, they share the smoother?
                                   // Yes, smoother state is persistent.
                                   // If mode changes, it might jump or drift,
                                   // but it's fine. Actually, if we skip
                                   // getNextValue in one branch, it lags.
                                   // Better to update smoothers outside of mode
                                   // check, OR accept lag on mode switch. Mode
                                   // switch is rare. Lag is fine/good.
          float sOct2 = oct2Sm.getNextValue();
          float sMix = octMixSm.getNextValue();

          float wetSignal = (lowOct * sOct1 * 1.4f) + (highOct * sOct2 * 1.5f);

          // Clip
          wetSignal = std::tanh(wetSignal);

          for (int ch = 0; ch < chs; ++ch) {
            float dry = buffer.getSample(ch, i);
            buffer.setSample(ch, i, dry * (1.0f - sMix) + wetSignal * sMix);
          }
        }
      }
    } else {
      // Skip smoothing if off to keep state current?
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
        const float inAbs = std::abs(buffer.getSample(0, i));
        if (inAbs > envFollower)
          envFollower = inAbs + attackCoef * (envFollower - inAbs);
        else
          envFollower = inAbs + releaseCoef * (envFollower - inAbs);

        float cutoff = 200.0f + 5000.0f * envRange01 * envFollower;
        cutoff = juce::jlimit(50.0f, 10000.0f, cutoff);

        envFilter.setCutoffFrequency(cutoff);
        envFilter.setResonance(8.0f);

        for (int ch = 0; ch < chs; ++ch) {
          float s = buffer.getSample(ch, i);
          s = envFilter.processSample(ch, s);
          buffer.setSample(ch, i, s);
        }
      }
    }
  }

private:
  void ensureScratch(int needed) {
    if (needed <= maxBlockSize)
      return;
    dryScratch.assign((size_t)needed, 0.0f);
    subScratch.assign((size_t)needed, 0.0f);
    maxBlockSize = needed;
  }

  float sampleRate{44100.0f};
  int maxBlockSize{512};
  int numChannels{2};
  bool prepared{false};

  PitchShifter shifter; // Modern Mode Engine
  std::vector<float> dryScratch;
  std::vector<float> subScratch;

  // Parameters
  bool octOn = false;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oct1Sm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> oct2Sm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> octMixSm{0.0f};

  bool modernMode = false;
  bool envOn = false;
  float envAttack01 = 0.0f;
  float envDecay01 = 0.0f;
  float envRange01 = 0.0f;

  // Octaver State (Vintage)
  float octInputFilter{0.0f};
  float octLowFilter{0.0f};
  float octIntegrator{0.0f};
  float octEnv{0.0f};
  bool octFlip{false};
  bool octLastAbove{false};
  float octHighFilter{0.0f};
  float octHighSmooth{0.0f};

  // Envelope State
  juce::dsp::StateVariableTPTFilter<float> envFilter;
  float envFollower{0.0f};
};
