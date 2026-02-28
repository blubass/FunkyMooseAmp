#pragma once

#include "../JuceIncludes.h"
#include "AmpCabBlock.h" // New Wrapper
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
  using OctEnv = OctEnvModule;
  using InputGain = juce::dsp::Gain<float>;
  // using AmpTone = AmpToneModule; -> Now inside AmpCabBlock
  // using CabSim = CabSim;         -> Now inside AmpCabBlock
  using LowCut =
      juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                     juce::dsp::IIR::Coefficients<float>>;
  using AmpCab = AmpCabBlock;
  using Compressor = CompressorModule; // Includes Punch
  using ModFX = ModFxModule;
  using Mojo = FxModule; // Includes Mojo/Oversampling
  using SmartGateBlock = SmartGate;
  using OutputGain =
      OutputModule; // OutputModule handles gain + safety clip + metering

  // The Processor Chain
  // Structure: SmartGate -> OctEnv -> LowCut -> InputGain -> [Amp -> Cab
  // (Oversampled)] -> Compressor
  // -> ModFX -> Mojo -> OutputGain
  using Chain =
      juce::dsp::ProcessorChain<SmartGateBlock, OctEnv, LowCut, InputGain,
                                AmpCab, Compressor, ModFX, Mojo, OutputGain>;

  MooseDSPChain() = default;

  void prepare(const juce::dsp::ProcessSpec &spec) {
    chain.prepare(spec);

    // Ramping for InputGain (juce::dsp::Gain) - Index 3
    chain.get<3>().setRampDurationSeconds(0.05);
  }

  void reset() { chain.reset(); }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    chain.process(ctx);
  }

  float getLatency() const {
    // Sum latencies of blocks that introduce latency (Oversampling/FFT)
    // OctEnv (Index 1), AmpCabBlock (Index 4) and Mojo (Index 7)
    return (float)chain.get<1>().getLatencyInSamples() +
           chain.get<4>().getLatency() +
           (float)chain.get<7>().getLatencyInSamples();
  }

  // Accessors to modules for parameter updates
  OctEnv &getOctEnv() { return chain.get<1>(); }
  LowCut &getLowCut() { return chain.get<2>(); }
  void setLowCutBypassed(bool b) { chain.setBypassed<2>(b); }

  InputGain &getInputGain() { return chain.get<3>(); }
  AmpCabBlock &getAmpCabBlock() { return chain.get<4>(); }

  // Proxy accessors for compatibility
  AmpToneModule &getAmpTone() { return chain.get<4>().getAmp(); }
  CabSim &getCabSim() { return chain.get<4>().getCab(); }

  Compressor &getCompressor() { return chain.get<5>(); }
  ModFX &getModFX() { return chain.get<6>(); }
  Mojo &getMojo() { return chain.get<7>(); }
  OutputGain &getOutputGain() { return chain.get<8>(); }
  SmartGate &getSmartGate() { return chain.get<0>(); }
  const SmartGate &getSmartGate() const { return chain.get<0>(); }

  const OctEnv &getOctEnv() const { return chain.get<1>(); }
  const InputGain &getInputGain() const { return chain.get<3>(); }

  const AmpToneModule &getAmpTone() const { return chain.get<4>().getAmp(); }
  const CabSim &getCabSim() const { return chain.get<4>().getCab(); }

  const Compressor &getCompressor() const { return chain.get<5>(); }
  const ModFX &getModFX() const { return chain.get<6>(); }
  const Mojo &getMojo() const { return chain.get<7>(); }
  const OutputGain &getOutputGain() const { return chain.get<8>(); }

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
