#pragma once

#include "../JuceIncludes.h"
#include "AmpBlock.h" // New Wrapper (Oversampled Amp only)
#include "AmpToneModule.h"
#include "CabSim.h"
#include "CompressorModule.h"
#include "FxModule.h" // Mojo
#include "ModFxModule.h"
#include "OctEnvModule.h"
#include "OutputModule.h"
#include "SmartGate.h"

//==============================================================================
/**
 * Central DSP Engine for Funky Moose.
 * Encapsulates the entire processing chain.
 */
class MooseDSPChain {
public:
  // Define the module types for the chain
  using SmartGateBlock = SmartGate;
  using Compressor = CompressorModule; // Includes Punch
  using OctEnv = OctEnvModule;
  using LowCut =
      juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                     juce::dsp::IIR::Coefficients<float>>;
  using InputGain = juce::dsp::Gain<float>;
  using Amp = AmpBlock;
  using Mojo = FxModule; // Includes Mojo/Oversampling
  using ModFX = ModFxModule;
  using Cab = CabSim;
  using OutputGain =
      OutputModule; // OutputModule handles gain + safety clip + metering

  // The Processor Chain
  // Structure: Input -> Gate (0) -> Compressor (1) -> OctEnv (2) -> LowCut (3)
  // -> Gain (4)
  // -> Amp (Oversampled) (5) -> Mojo (6) -> ModFX (7) -> Cab (Base) (8) ->
  // Output (9)
  using Chain =
      juce::dsp::ProcessorChain<SmartGateBlock, Compressor, OctEnv, LowCut,
                                InputGain, Amp, Mojo, ModFX, Cab, OutputGain>;

  MooseDSPChain() = default;

  void prepare(const juce::dsp::ProcessSpec &spec) {
    chain.prepare(spec);

    // Ramping for InputGain (juce::dsp::Gain) - Index 4
    chain.get<4>().setRampDurationSeconds(0.05);
  }

  void reset() { chain.reset(); }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    chain.process(ctx);
  }

  float getLatency() const {
    // Sum latencies: OctEnv (Index 2), AmpBlock (Index 5) and Mojo (Index 6)
    return (float)chain.get<2>().getLatencyInSamples() +
           chain.get<5>().getLatency() +
           (float)chain.get<6>().getLatencyInSamples();
  }

  // Accessors to modules for parameter updates
  SmartGate &getSmartGate() { return chain.get<0>(); }
  const SmartGate &getSmartGate() const { return chain.get<0>(); }

  Compressor &getCompressor() { return chain.get<1>(); }
  const Compressor &getCompressor() const { return chain.get<1>(); }

  OctEnv &getOctEnv() { return chain.get<2>(); }
  const OctEnv &getOctEnv() const { return chain.get<2>(); }

  LowCut &getLowCut() { return chain.get<3>(); }
  void setLowCutBypassed(bool b) { chain.setBypassed<3>(b); }

  InputGain &getInputGain() { return chain.get<4>(); }
  const InputGain &getInputGain() const { return chain.get<4>(); }

  AmpBlock &getAmpBlock() { return chain.get<5>(); }
  AmpToneModule &getAmpTone() { return chain.get<5>().getAmp(); }
  const AmpToneModule &getAmpTone() const { return chain.get<5>().getAmp(); }

  Mojo &getMojo() { return chain.get<6>(); }
  const Mojo &getMojo() const { return chain.get<6>(); }

  ModFX &getModFX() { return chain.get<7>(); }
  const ModFX &getModFX() const { return chain.get<7>(); }

  CabSim &getCabSim() { return chain.get<8>(); }
  const CabSim &getCabSim() const { return chain.get<8>(); }

  OutputGain &getOutputGain() { return chain.get<9>(); }
  const OutputGain &getOutputGain() const { return chain.get<9>(); }

  template <int index> void setBypassed(bool bypassed) {
    chain.setBypassed<index>(bypassed);
  }

  template <int index> void setBypassedAndReset(bool bypassed) {
    if (chain.isBypassed<index>() != bypassed) {
      chain.setBypassed<index>(bypassed);
      chain.get<index>().reset();
    }
  }

private:
  Chain chain;
};
