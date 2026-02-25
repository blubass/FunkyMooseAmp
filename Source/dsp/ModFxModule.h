#pragma once

#include "../JuceIncludes.h"

//==============================================================================
// Modulation FX Module: Phaser + Chorus
//
// - Header-only, no extra CMake changes.
// - Keeps all phaser/chorus state out of PluginProcessor.
// - Uses JUCE dsp::Phaser and dsp::Chorus.
//
class ModFxModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    phaser.reset();
    chorus.reset();

    phaser.prepare(spec);
    chorus.prepare(spec);

    // Chorus defaults (TC-ish fast)
    chorus.setCentreDelay(15.0f);
    chorus.setMix(0.5f);
    chorus.setFeedback(0.0f);

    // Init Smoothers
    phMixSm.reset(spec.sampleRate, 0.05); // 50ms ramp
    chMixSm.reset(spec.sampleRate, 0.05);

    // Prepare temp buffer for parallel processing
    tempBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    prepared = true;
  }

  void reset() {
    phaser.reset();
    chorus.reset();
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    updateParams();

    // PARALLEL VS SERIAL
    if (isParallel && phOn && chOn) {
      // --- PARALLEL MODE ---
      // 1. Copy Input to Temp
      auto mainBlock = ctx.getOutputBlock();
      juce::dsp::AudioBlock<float> tempBlock(tempBuffer);

      // Use sub-block of temp buffer matching current sample count
      auto tempSub = tempBlock.getSubBlock(0, mainBlock.getNumSamples());
      tempSub.copyFrom(mainBlock);

      // 2. Process Phaser on Main
      phaser.process(ctx);

      // 3. Process Chorus on Temp
      juce::dsp::ProcessContextReplacing<float> tempCtx(tempSub);
      chorus.process(tempCtx);

      // 4. Sum (Parallel Mix)
      // Note: This sums Dry signals too if Mix < 100%.
      // For pure parallel effects, users might want 100% wet on modules?
      // We'll leave that to the user's knob settings.
      mainBlock.add(tempSub);

      // Optional: Normalize gain? No, let it be loud/thick.

    } else {
      // --- SERIAL MODE (Default) ---
      // Phaser -> Chorus
      if (phOn) {
        phaser.process(ctx);
      } else {
        phMixSm.skip(ctx.getOutputBlock().getNumSamples());
      }

      if (chOn) {
        chorus.process(ctx);
      } else {
        chMixSm.skip(ctx.getOutputBlock().getNumSamples());
      }
    }
  }

  void updateParams() {
    if (phOn) {
      phaser.setRate(juce::jmap(phRate01, 0.1f, 10.0f));
      phaser.setCentreFrequency(1000.0f);
      phaser.setFeedback(juce::jmap(phColour01, -0.6f, 0.6f));
      phaser.setMix(phMixSm.getNextValue()); // updates smoother
    }
    if (chOn) {
      chorus.setRate(juce::jmap(chRate01, 0.1f, 5.0f));
      chorus.setDepth(juce::jlimit(0.0f, 1.0f, chDepth01));
      chorus.setCentreDelay(15.0f);
      chorus.setFeedback(0.0f);
      chorus.setMix(chMixSm.getNextValue()); // updates smoother
    }
  }

  void setPhaserOn(bool on) noexcept { phOn = on; }
  void setPhaserRate(float v) noexcept { phRate01 = v; }
  void setPhaserMix(float v) noexcept { phMixSm.setTargetValue(v); }
  void setPhaserColour(float v) noexcept { phColour01 = v; }

  void setChorusOn(bool on) noexcept { chOn = on; }
  void setChorusRate(float v) noexcept { chRate01 = v; }
  void setChorusDepth(float v) noexcept { chDepth01 = v; }
  void setChorusMix(float v) noexcept { chMixSm.setTargetValue(v); }

  void setParallel(bool parallel) noexcept { isParallel = parallel; }

private:
  juce::dsp::Phaser<float> phaser;
  juce::dsp::Chorus<float> chorus;
  bool prepared{false};

  // Parameters
  bool phOn = false;
  float phRate01 = 0.5f;
  float phColour01 = 0.5f;

  bool chOn = false;
  float chRate01 = 0.5f;
  float chDepth01 = 0.5f;

  // Smoothing for Mix parameters to avoid clicks
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> phMixSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> chMixSm{0.0f};

  // Parallel processing support
  bool isParallel = false;
  juce::AudioBuffer<float> tempBuffer;
};
