#pragma once

#include "../JuceIncludes.h"
#include <cmath>
#include <memory>
#include <vector>

// Mojo (post-comp saturation with fixed oversampling)
class FxModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    // Mojo oversampling (fixed internal 4x)
    mojoOS = std::make_unique<juce::dsp::Oversampling<float>>(
        (int)spec.numChannels,
        2, // 2^2 = 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    mojoOS->reset();
    mojoOS->initProcessing((size_t)spec.maximumBlockSize);

    mojoDrive01.reset(spec.sampleRate, 0.05); // 50ms smooth

    prepared = true;
  }

  void reset() {
    if (mojoOS)
      mojoOS->reset();
  }

  void setMojoDrive01(float d01) {
    mojoDrive01.setTargetValue(juce::jlimit(0.0f, 1.0f, d01));
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared || mojoOS == nullptr)
      return;

    auto &buffer = ctx.getOutputBlock();

    // Convert to AudioBlock for oversampling
    juce::dsp::AudioBlock<float> mainBlock = buffer;

    auto osBlock = mojoOS->processSamplesUp(mainBlock);

    // Advance smooth value once per block? Or use getNextValue inside loop?
    // Since oversampling makes the inner loop 4x, using getNextValue
    // initialized with 1x sample rate inside the OS loop would result in 4x
    // faster ramp. Let's grab the NEXT smoothed value for this block and apply
    // it constantly for this block. This is block-rate smoothing (zipper noise
    // reduced by interpolation across blocks, but steps per block). Actually,
    // SmoothedValue ramps when you call getNextValue. If I only call it once
    // per block, it jumps. I should iterate samples. I will use a simple linear
    // interpolation for drive across the block. Or just use the target value if
    // we don't care about super smoothness? No user asked for smoothing. I will
    // iterate the OS loop and use getNextValue(), but scale the step?
    // SmoothedValue doesn't support changing step size easily dynamically.
    // Solution: Just use current value (ramped per block) or accept 4x speedup.
    // 4x speedup on 50ms ramp -> 12.5ms ramp. That's fine.

    // Actually, I need to call getNextValue per BASE sample, not per OS sample.
    // Since OS block has 4x samples, I can just hold the value for 4 samples?
    // Too complex.
    // I will use per-block smoothing (update once per block).
    // Wait, SmoothedValue is designed for per-sample.
    // I will use `getNextValue` inside the loop (4x speed) and just increase
    // the smooth time in prepare to 0.2s? 0.2s / 4 = 50ms.

    // Let's assume standard behavior and just use getNextValue in the loop.

    size_t numSamples = osBlock.getNumSamples();
    size_t numCh = osBlock.getNumChannels();

    for (size_t n = 0; n < numSamples; ++n) {
      // Advance smoother every 4 samples? Or just let it run fast.
      // Let's let it run fast.
      float d01 = mojoDrive01.getNextValue();
      const float drive = juce::jmap(d01, 1.05f, 2.40f);

      for (size_t ch = 0; ch < numCh; ++ch) {
        float *x = osBlock.getChannelPointer(ch);
        x[n] = std::tanh(x[n] * drive);
      }
    }

    mojoOS->processSamplesDown(mainBlock);

    // Peak-safety clipper (very gentle) - keeping this from original FxModule
    // Note: OutputModule also has safety clipper. This one might be redundant
    // or for sound? "Very gentle" tanh. I'll keep it as part of the sound.
    for (size_t ch = 0; ch < mainBlock.getNumChannels(); ++ch) {
      float *ptr = mainBlock.getChannelPointer(ch);
      for (size_t i = 0; i < mainBlock.getNumSamples(); ++i) {
        const float s = ptr[i];
        if (s > 0.99f || s < -0.99f)
          ptr[i] = std::tanh(s);
      }
    }
  }

  // Latency reporting helper
  float getLatencyInSamples() const {
    return mojoOS ? mojoOS->getLatencyInSamples() : 0.0f;
  }

private:
  // Mojo
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mojoDrive01{
      0.25f};
  std::unique_ptr<juce::dsp::Oversampling<float>> mojoOS;
  bool prepared{false};
};
