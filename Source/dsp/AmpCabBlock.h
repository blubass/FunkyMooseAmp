#pragma once

#include "../JuceIncludes.h"
#include "AmpToneModule.h"
#include "CabSim.h"

// Wrapper for Amp + Cab with Oversampling
class AmpCabBlock {
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

    // Cab processes at base sample rate
    cab.prepare(spec);
  }

  void reset() {
    if (os)
      os->reset();
    amp.reset();
    cab.reset();
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

    // 2. Process Cab at BASE sample rate (much more efficient)
    juce::dsp::ProcessContextReplacing<float> baseCtx(block);
    cab.process(baseCtx);
  }

  AmpToneModule &getAmp() { return amp; }
  CabSim &getCab() { return cab; }

  const AmpToneModule &getAmp() const { return amp; }
  const CabSim &getCab() const { return cab; }

  float getLatency() const { return os ? os->getLatencyInSamples() : 0.0f; }

private:
  AmpToneModule amp;
  CabSim cab;
  std::unique_ptr<juce::dsp::Oversampling<float>> os;
};
