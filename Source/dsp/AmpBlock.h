#pragma once

#include "../JuceIncludes.h"
#include "AmpToneModule.h"

// Wrapper for Amp with Oversampling
class AmpBlock {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    os.reset(new juce::dsp::Oversampling<float>(
        spec.numChannels, 2, // 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
    os->initProcessing(spec.maximumBlockSize);

    juce::dsp::ProcessSpec osSpec = spec;
    osSpec.sampleRate *= os->getOversamplingFactor();
    osSpec.maximumBlockSize =
        spec.maximumBlockSize * (int)os->getOversamplingFactor();

    amp.prepare(osSpec);
  }

  void reset() {
    if (os)
      os->reset();
    amp.reset();
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!os)
      return;

    juce::dsp::AudioBlock<float> block = ctx.getOutputBlock();

    // Oversample always to keep constant latency and avoid clicks
    juce::dsp::AudioBlock<float> osBlock = os->processSamplesUp(block);
    juce::dsp::ProcessContextReplacing<float> osCtx(osBlock);
    amp.process(osCtx);
    os->processSamplesDown(block);
  }

  AmpToneModule &getAmp() { return amp; }
  const AmpToneModule &getAmp() const { return amp; }

  float getLatency() const { return os ? os->getLatencyInSamples() : 0.0f; }

private:
  AmpToneModule amp;
  std::unique_ptr<juce::dsp::Oversampling<float>> os;
};
