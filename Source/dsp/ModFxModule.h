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
    chorusCrossover.prepare(spec);

    // Chorus defaults (TC-ish fast)
    chorus.setCentreDelay(15.0f);
    chorus.setMix(0.5f);
    chorus.setFeedback(0.0f);
    chorusCrossover.setCutoffFrequency(chCrossoverHz);

    // Init Smoothers
    phMixSm.reset(spec.sampleRate, 0.05); // 50ms ramp
    chMixSm.reset(spec.sampleRate, 0.05);

    // Smoothers for On/Off to avoid clicks
    phOnSm.reset(spec.sampleRate, 0.02);
    chOnSm.reset(spec.sampleRate, 0.02);

    // Prepare temp buffer for parallel processing
    tempBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    chorusLowBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    prepared = true;
  }

  void reset() {
    phaser.reset();
    chorus.reset();
    chorusCrossover.reset();
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    const float phOnLevel = phOnSm.getNextValue();
    const float chOnLevel = chOnSm.getNextValue();
    const bool isPhActuallyOn = phOnLevel > 0.001f || phOnSm.isSmoothing();
    const bool isChActuallyOn = chOnLevel > 0.001f || chOnSm.isSmoothing();

    if (!isPhActuallyOn && !isChActuallyOn) {
      phMixSm.skip(ctx.getOutputBlock().getNumSamples());
      chMixSm.skip(ctx.getOutputBlock().getNumSamples());
      return;
    }

    updateParams();

    // PARALLEL VS SERIAL
    if (isParallel && isPhActuallyOn && isChActuallyOn) {
      auto mainBlock = ctx.getOutputBlock();
      juce::dsp::AudioBlock<float> tempBlock(tempBuffer);
      auto tempSub = tempBlock.getSubBlock(0, mainBlock.getNumSamples());
      tempSub.copyFrom(mainBlock);

      phaser.process(ctx);
      mainBlock.multiplyBy(phOnLevel);

      processChorusBand(tempSub);
      tempSub.multiplyBy(chOnLevel);

      mainBlock.add(tempSub);
      mainBlock.multiplyBy(0.5f);
    } else {
      if (isPhActuallyOn) {
        auto mainBlock = ctx.getOutputBlock();
        tempBuffer.clear();
        juce::dsp::AudioBlock<float> tempBlock(tempBuffer);
        auto sub = tempBlock.getSubBlock(0, mainBlock.getNumSamples());
        sub.copyFrom(mainBlock);

        phaser.process(ctx);

        for (int ch = 0; ch < (int)mainBlock.getNumChannels(); ++ch) {
          auto *out = mainBlock.getChannelPointer(ch);
          auto *dry = sub.getChannelPointer(ch);
          for (int i = 0; i < (int)mainBlock.getNumSamples(); ++i) {
            out[i] = dry[i] + (out[i] - dry[i]) * phOnLevel;
          }
        }
      }

      if (isChActuallyOn) {
        auto mainBlock = ctx.getOutputBlock();
        tempBuffer.clear();
        juce::dsp::AudioBlock<float> tempBlock(tempBuffer);
        auto sub = tempBlock.getSubBlock(0, mainBlock.getNumSamples());
        sub.copyFrom(mainBlock);

        processChorusBand(mainBlock);

        for (int ch = 0; ch < (int)mainBlock.getNumChannels(); ++ch) {
          auto *out = mainBlock.getChannelPointer(ch);
          auto *dry = sub.getChannelPointer(ch);
          for (int i = 0; i < (int)mainBlock.getNumSamples(); ++i) {
            out[i] = dry[i] + (out[i] - dry[i]) * chOnLevel;
          }
        }
      }
    }
  }

  void updateParams() {
    float phMix = phMixSm.getNextValue();
    float chMix = chMixSm.getNextValue();

    // Only update Rate/Feedback if they haven't been set recently or changed
    // These maps are fast, but the underlying dsp classes might do re-allocs or
    // heavy state resets
    if (std::abs(phRate01 - lastPhRate) > 0.001f ||
        std::abs(phColour01 - lastPhCol) > 0.001f) {
      phaser.setRate(juce::jmap(phRate01, 0.1f, 10.0f));
      phaser.setFeedback(juce::jmap(phColour01, -0.6f, 0.6f));
      lastPhRate = phRate01;
      lastPhCol = phColour01;
    }
    phaser.setMix(phMix);

    if (std::abs(chRate01 - lastChRate) > 0.001f ||
        std::abs(chDepth01 - lastChDepth) > 0.001f) {
      chorus.setRate(juce::jmap(chRate01, 0.1f, 5.0f));
      chorus.setDepth(juce::jlimit(0.0f, 1.0f, chDepth01));
      lastChRate = chRate01;
      lastChDepth = chDepth01;
    }
    chorus.setMix(chMix);
  }

  void setPhaserOn(bool on) noexcept {
    phOnSm.setTargetValue(on ? 1.0f : 0.0f);
  }
  void setPhaserRate(float v) noexcept { phRate01 = v; }
  void setPhaserMix(float v) noexcept { phMixSm.setTargetValue(v); }
  void setPhaserColour(float v) noexcept { phColour01 = v; }

  void setChorusOn(bool on) noexcept {
    chOnSm.setTargetValue(on ? 1.0f : 0.0f);
  }
  void setChorusRate(float v) noexcept { chRate01 = v; }
  void setChorusDepth(float v) noexcept { chDepth01 = v; }
  void setChorusMix(float v) noexcept { chMixSm.setTargetValue(v); }
  void setChorusCrossover(float hz) noexcept {
    chCrossoverHz = juce::jlimit(80.0f, 800.0f, hz);
  }

  void setParallel(bool parallel) noexcept { isParallel = parallel; }

private:
  void updateChorusCrossover() {
    if (std::abs(chCrossoverHz - lastChCrossoverHz) > 0.5f) {
      chorusCrossover.setCutoffFrequency(chCrossoverHz);
      lastChCrossoverHz = chCrossoverHz;
    }
  }

  void processChorusBand(juce::dsp::AudioBlock<float> block) {
    updateChorusCrossover();

    juce::dsp::AudioBlock<float> lowBlock(chorusLowBuffer);
    auto lowSub = lowBlock.getSubBlock(0, block.getNumSamples());

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
      auto *samples = block.getChannelPointer(ch);
      auto *low = lowSub.getChannelPointer(ch);

      for (size_t i = 0; i < block.getNumSamples(); ++i) {
        float lowSample = 0.0f;
        float highSample = 0.0f;
        chorusCrossover.processSample((int)ch, samples[i], lowSample,
                                      highSample);
        low[i] = lowSample;
        samples[i] = highSample;
      }
    }

    juce::dsp::ProcessContextReplacing<float> chorusCtx(block);
    chorus.process(chorusCtx);
    block.add(lowSub);
  }

  juce::dsp::Phaser<float> phaser;
  juce::dsp::Chorus<float> chorus;
  juce::dsp::LinkwitzRileyFilter<float> chorusCrossover;
  bool prepared{false};

  // Smoothers for Mix parameters
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> phMixSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> chMixSm{0.0f};

  // Smoothers for On/Off bypass
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> phOnSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> chOnSm{0.0f};

  float phRate01 = 0.5f;
  float phColour01 = 0.5f;
  float chRate01 = 0.5f;
  float chDepth01 = 0.5f;
  float chCrossoverHz = 180.0f;

  // Cache for parameter updates
  float lastPhRate = -1.0f;
  float lastPhCol = -1.0f;
  float lastChRate = -1.0f;
  float lastChDepth = -1.0f;
  float lastChCrossoverHz = -1.0f;

  bool isParallel = false;
  juce::AudioBuffer<float> tempBuffer;
  juce::AudioBuffer<float> chorusLowBuffer;
};
