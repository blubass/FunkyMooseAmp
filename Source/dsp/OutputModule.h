#pragma once

#include "../JuceIncludes.h"
#include <atomic>
#include <cmath>

//==============================================================================
// Output / Master Module
//
// Responsibilities:
// - Apply output gain (post everything).
// - Peak safety clip (gentle).
// - Provide RMS metering taps (input RMS and output RMS) to the processor.
// - Mono Maker (Side HPF).
// - Auto Gain Compensation.
//
//==============================================================================
class OutputModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    outGain.reset();
    outGain.prepare(spec);
    outGain.setRampDurationSeconds(0.05);

    // Mono Maker (High pass on Side channel)
    sideHighPass.prepare(spec);
    sideHighPass.reset();

    sampleRate = spec.sampleRate;
    prepared = true;
  }

  void reset() {
    outGain.reset();
    sideHighPass.reset();
    inRmsSmooth = 0.0f;
    outRmsSmooth = 0.0f;
    autoGainComp = 1.0f;
  }

  void setMonoMakerFreq(float freq) noexcept {
    monoMakerFreq = freq;
    updateMonoMaker();
  }

  void setMonoMakerEnabled(bool enabled) noexcept {
    monoMakerEnabled = enabled;
  }

  void setAutoGain(bool enabled) noexcept { autoGainEnabled = enabled; }
  void setGainDecibels(float db) noexcept { outDb = db; }
  void setSafetyClipThreshold(float th) noexcept { safetyClipThreshold = th; }
  void resetAutoGainComp() noexcept { autoGainComp = 1.0f; }

  // Thread-safe access for UI
  float getInRms() const noexcept {
    return inRmsAtomic.load(std::memory_order_relaxed);
  }
  float getOutRms() const noexcept {
    return outRmsAtomic.load(std::memory_order_relaxed);
  }

  // Call once per block at the end of the chain.
  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    // 0. Mono Maker (Pre Output Gain)
    if (monoMakerEnabled && monoMakerFreq > 20.0f) {
      juce::dsp::AudioBlock<float> block = ctx.getOutputBlock();
      if (block.getNumChannels() == 2) {
        // Convert L/R -> M/S
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
          float l = block.getSample(0, (int)i);
          float r = block.getSample(1, (int)i);
          float m = (l + r) * 0.5f;
          float s = (l - r) * 0.5f;

          block.setSample(0, (int)i, m);
          block.setSample(1, (int)i, s);
        }

        // High Pass filter on Side (Channel 1)
        auto sideBlock = block.getSingleChannelBlock(1);
        juce::dsp::ProcessContextReplacing<float> sideCtx(sideBlock);
        sideHighPass.process(sideCtx);

        // Convert M/S -> L/R
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
          float m = block.getSample(0, (int)i);
          float s = block.getSample(1, (int)i);
          float l = m + s;
          float r = m - s;

          block.setSample(0, (int)i, l);
          block.setSample(1, (int)i, r);
        }
      }
    }

    // Output gain + Auto Gain Compensation
    float finalDb = outDb;
    if (autoGainEnabled) {
      const float inLvl = inRmsSmooth + 0.0001f;
      const float outLvl = outRmsSmooth / (autoGainComp + 0.0001f) + 0.0001f;

      float targetGain = inLvl / (outLvl + 0.0001f);
      targetGain = juce::jlimit(0.1f, 10.0f, targetGain);

      const float alpha = 0.995f;
      autoGainComp = autoGainComp * alpha + targetGain * (1.0f - alpha);

      finalDb += juce::Decibels::gainToDecibels(autoGainComp);
    }

    outGain.setGainDecibels(finalDb);
    outGain.process(ctx);

    auto &buffer = ctx.getOutputBlock();

    // Safety clip
    const float thr = juce::jlimit(0.2f, 0.9995f, safetyClipThreshold);
    const int n = (int)buffer.getNumSamples();
    const int chs = (int)buffer.getNumChannels();

    float inSum = 0.0f;
    float outSum = 0.0f;

    for (int ch = 0; ch < chs; ++ch) {
      auto *x = buffer.getChannelPointer(ch);
      for (int i = 0; i < n; ++i) {
        const float vPre = x[i];
        inSum += vPre * vPre;
        float v = vPre;
        if (std::abs(v) > thr) {
          const float s = (v >= 0.0f) ? 1.0f : -1.0f;
          const float t = (std::abs(v) - thr) / (1.0f - thr + 1.0e-6f);
          v = s * (thr +
                   (1.0f - thr) *
                       std::tanh(t)); // Fixed: Removed * 2.0f for C1 continuity
        }
        x[i] = v;
        outSum += v * v;
      }
    }

    // RMS
    const float denom = float(juce::jmax(1, n * chs));
    const float inRms = std::sqrt(inSum / denom);
    const float outRms = std::sqrt(outSum / denom);
    const float tau = 0.02f; // Faster RMS for snappier meters
    const float alpha = 1.0f - std::exp(-float(n) / float(sampleRate * tau));
    inRmsSmooth += alpha * (inRms - inRmsSmooth);
    outRmsSmooth += alpha * (outRms - outRmsSmooth);
    inRmsAtomic.store(inRmsSmooth, std::memory_order_relaxed);
    outRmsAtomic.store(outRmsSmooth, std::memory_order_relaxed);
  }

private:
  juce::dsp::Gain<float> outGain;
  double sampleRate{44100.0};
  bool prepared{false};

  float inRmsSmooth{0.0f};
  float outRmsSmooth{0.0f};

  float outDb = 0.0f;
  float safetyClipThreshold = 1.0f;
  std::atomic<float> inRmsAtomic{0.0f};
  std::atomic<float> outRmsAtomic{0.0f};

  // Mono Maker
  float monoMakerFreq = 0.0f;
  bool monoMakerEnabled = true;
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                 juce::dsp::IIR::Coefficients<float>>
      sideHighPass;

  // Auto Gain
  bool autoGainEnabled = false;
  float autoGainComp = 1.0f;

  void updateMonoMaker() {
    if (sampleRate > 0.0) {
      *sideHighPass.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
          sampleRate, monoMakerFreq);
    }
  }
};
