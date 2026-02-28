#pragma once

#include "../JuceIncludes.h"
#include "SagModule.h"

//==============================================================================
// Solid-State Soft Clip (Mark's Method)
//
static inline float markSoftClip(float x, float drive) {
  // drive: 1.0 bis 2.0 (empfohlen)
  const float a = drive;

  // sehr "clean" Softclip: atan-Form, wenig Farbe
  float y = (2.0f / juce::MathConstants<float>::pi) * std::atan(a * x);

  // sehr kleine 3rd harmonic Würze (odd harmonics, solid-state feel)
  y += 0.03f * (x * x * x);

  return y;
}

//==============================================================================
// Amp + Tone + Slap Module
//
// - Owns input gain and tone filters (bass/mid/treble + slap scoop).
// - Keeps PluginProcessor clean.
// - No heap allocations in process().
//
class AmpToneModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;

    bass.reset();
    mid.reset();
    treble.reset();
    slap.reset();
    dcBlocker.reset();

    bassDbSm.reset(spec.sampleRate, 0.05); // 50ms smooth
    midDbSm.reset(spec.sampleRate, 0.05);
    trebleDbSm.reset(spec.sampleRate, 0.05);
    inputGainSm.reset(spec.sampleRate, 0.05);
    ampOnSm.reset(spec.sampleRate, 0.02);   // 20ms fade
    tubeOnSm.reset(spec.sampleRate, 0.02);  // 20ms fade
    slapMixSm.reset(spec.sampleRate, 0.05); // 50ms fade for slap

    bass.prepare(spec);
    mid.prepare(spec);
    treble.prepare(spec);
    slap.prepare(spec);
    dcBlocker.prepare(spec);

    // Pre-Saturation Filters
    preHPF.prepare(spec);
    prePresence.prepare(spec);

    // De-Saturation Filters
    deLPF.prepare(spec);
    deHarshDip.prepare(spec);

    // Subsonic HPF (protects from DC before saturation)
    if (dcBlocker.state == nullptr)
      dcBlocker.state = new juce::dsp::IIR::Coefficients<float>();
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        spec.sampleRate, 25.0f);

    // Pre-Emphasis: HPF 30 Hz
    if (preHPF.state == nullptr)
      preHPF.state = new juce::dsp::IIR::Coefficients<float>();
    *preHPF.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        spec.sampleRate, 30.0f);

    // Pre-Emphasis: Presence Peak (+1.5 dB @ 2 kHz, Q=1.0, broad)
    if (prePresence.state == nullptr)
      prePresence.state = new juce::dsp::IIR::Coefficients<float>();
    *prePresence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        spec.sampleRate, 2000.0f, 0.8f, juce::Decibels::decibelsToGain(1.5f));

    // De-Emphasis: LPF 14 kHz (smooth, no digital artifacts)
    if (deLPF.state == nullptr)
      deLPF.state = new juce::dsp::IIR::Coefficients<float>();
    *deLPF.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        spec.sampleRate, 14000.0f);

    // De-Emphasis: Optional Harsh Dip at 3.5 kHz (-2 dB, wide Q=0.7)
    if (deHarshDip.state == nullptr)
      deHarshDip.state = new juce::dsp::IIR::Coefficients<float>();
    *deHarshDip.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        spec.sampleRate, 3500.0f, 0.7f, juce::Decibels::decibelsToGain(-2.0f));

    // DC removal filter coefficient (5 Hz, 1-pole)
    dcRemovalCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi *
                                     5.0f / (float)spec.sampleRate);

    // Sag Module (default: amount=0.08, release=120ms)
    sagModule.prepare(spec);
    sagModule.setSagAmount(0.08f);
    sagModule.setReleaseMs(120.0f);

    scratchBuffer.setSize(spec.numChannels, spec.maximumBlockSize);

    prepared = true;
  }

  void setAmpOn(bool shouldBeOn) noexcept {
    ampOnSm.setTargetValue(shouldBeOn ? 1.0f : 0.0f);
  }
  bool isAmpOn() const noexcept { return ampOnSm.getTargetValue() > 0.5f; }
  bool isAmpActuallyActive() const noexcept {
    return ampOnSm.getCurrentValue() > 0.0001f || ampOnSm.isSmoothing();
  }
  void setTubeOn(bool shouldBeOn) noexcept {
    tubeOnSm.setTargetValue(shouldBeOn ? 1.0f : 0.0f);
  }
  void setSlapOn(bool shouldBeOn) noexcept {
    slapOn = shouldBeOn;
    slapMixSm.setTargetValue(shouldBeOn ? 1.0f : 0.0f);
  }
  void setInputGainDb(float db) noexcept {
    inputGainDb = juce::jlimit(-6.0f, 12.0f, db);
    inputGainSm.setTargetValue(juce::Decibels::decibelsToGain(inputGainDb));
  }
  void setBassDb(float db) noexcept { bassDbSm.setTargetValue(db); }
  void setMidDb(float db) noexcept { midDbSm.setTargetValue(db); }
  void setTrebleDb(float db) noexcept { trebleDbSm.setTargetValue(db); }
  void setAutoGain(bool enabled) noexcept { autoGainEnabled = enabled; }
  void setSaturationDrive(float d) noexcept {
    saturationDrive = juce::jlimit(1.0f, 2.0f, d);
  }
  void setSagAttackMs(float ms) noexcept { sagModule.setAttackMs(ms); }
  void setSagReleaseMs(float ms) noexcept { sagModule.setReleaseMs(ms); }
  void setSagAmount(float amount) noexcept { sagModule.setSagAmount(amount); }
  void setHarshDipEnabled(bool enabled) noexcept { harshDipEnabled = enabled; }

  void reset() {
    bass.reset();
    mid.reset();
    treble.reset();
    slap.reset();
    dcBlocker.reset();
    preHPF.reset();
    prePresence.reset();
    deLPF.reset();
    deHarshDip.reset();
    dcFilter[0] = 0.0f;
    dcFilter[1] = 0.0f;
    sagModule.reset();
    ampOnSm.setCurrentAndTargetValue(ampOnSm.getTargetValue());
    tubeOnSm.setCurrentAndTargetValue(tubeOnSm.getTargetValue());
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples = (int)ctx.getOutputBlock().getNumSamples();

    // 1. Advance Smoothed Values for EQ (skip to end of block for efficiency)
    bassDbSm.skip(numSamples);
    midDbSm.skip(numSamples);
    trebleDbSm.skip(numSamples);
    slapMixSm.skip(numSamples);

    // Update Coefficients
    updateFilters();

    // 2. Amp Simulation (Tube Sat + EQ)
    const bool ampIsActive = isAmpActuallyActive();
    if (ampIsActive) {
      auto &block = ctx.getOutputBlock();
      const int numSamples = (int)block.getNumSamples();
      const int numCh = (int)block.getNumChannels();

      // Ensure scratch buffer is large enough
      if (scratchBuffer.getNumSamples() < numSamples)
        scratchBuffer.setSize(numCh, numSamples, false, false, true);

      // --- CAPTURE TRUE DRY SIGNAL BEFORE ANY PROCESSING ---
      for (int ch = 0; ch < numCh; ++ch)
        scratchBuffer.copyFrom(ch, 0, block.getChannelPointer(ch), numSamples);

      // --- MOOSE PREAMP: Transformer Feel ---
      float peak = 0.0f;

      // Pre-Saturation Filters (at base rate)
      preHPF.process(ctx);
      prePresence.process(ctx);

      // Process saturation (Oversampling is now handled by the outer AmpBlock
      // at 4x)
      for (int i = 0; i < numSamples; ++i) {
        // CALL SMOOTHERS ONCE PER SAMPLE (Fixed stereo sync bug)
        const float currentGain = inputGainSm.getNextValue();
        const float currentTubeOn = tubeOnSm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch) {
          float *data = block.getChannelPointer(ch);
          float x = data[i] * currentGain;
          const float dryPreSat = x;

          // Tube Saturation (More aggressive, asymmetric for 'Tube' feel)
          const float drive = saturationDrive * 1.5f;
          float hb = x * drive;

          // Asymmetric shaper
          const float tubeBias = 0.15f;
          float y = std::tanh(hb + tubeBias) - std::tanh(tubeBias);

          // Add some 2nd harmonic (asymmetry) and 3rd harmonic (grid current)
          y += 0.06f * (hb * hb) + 0.04f * (hb * hb * hb);

          // DC Removal (1-pole HPF @ 5 Hz)
          dcFilter[ch] += (y - dcFilter[ch]) * dcRemovalCoeff;
          y -= dcFilter[ch];

          // Mix Tube Saturation (Smoothed)
          y = dryPreSat + (y - dryPreSat) * currentTubeOn;

          // Saturator compressed output
          data[i] = y * (0.85f - (currentTubeOn * 0.05f));

          float absY = std::abs(y);
          if (absY > peak)
            peak = absY;
        }
      }

      // Apply Sag
      sagModule.process(ctx);

      // De-Saturation Filters
      deLPF.process(ctx);
      if (harshDipEnabled)
        deHarshDip.process(ctx);

      // Subsonic protection
      dcBlocker.process(ctx);

      visualLevel.store(peak, std::memory_order_relaxed);

      // 3. Main Tone Stack
      bass.process(ctx);
      mid.process(ctx);
      treble.process(ctx);

      // Apply Slap filter with sample-accurate crossfade
      for (int ch = 0; ch < numCh; ++ch)
        scratchBuffer.copyFrom(ch, 0, block.getChannelPointer(ch), numSamples);

      slap.process(ctx); // Block is now wet

      for (int i = 0; i < numSamples; ++i) {
        float mix = slapMixSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch) {
          float dry = scratchBuffer.getSample(ch, i);
          float *wet = block.getChannelPointer(ch);
          wet[i] = dry + (wet[i] - dry) * mix;
        }
      }

      // Auto-Gain Compensation
      if (autoGainEnabled) {
        float eqComp = 0.0f;
        eqComp -= bassDbSm.getCurrentValue() * 0.15f;
        eqComp -= midDbSm.getCurrentValue() * 0.1f;
        eqComp -= trebleDbSm.getCurrentValue() * 0.08f;
        eqComp += slapMixSm.getCurrentValue() * 3.0f;

        float compGain = juce::Decibels::decibelsToGain(eqComp);
        block.multiplyBy(compGain);
      }

      // Final Crossfade for Amp Bypass
      const float startMix = ampOnSm.getCurrentValue();
      ampOnSm.skip(numSamples);
      const float endMix = ampOnSm.getCurrentValue();

      if (startMix < 0.999f || endMix < 0.999f) {
        for (int ch = 0; ch < numCh; ++ch) {
          float *d = block.getChannelPointer(ch);
          const float *dry = scratchBuffer.getReadPointer(ch);
          for (int i = 0; i < numSamples; ++i) {
            float progress = (float)i / (float)numSamples;
            float mix = startMix + (endMix - startMix) * progress;
            d[i] = dry[i] + (d[i] - dry[i]) * mix;
          }
        }
      }
    } else {
      visualLevel.store(0.0f, std::memory_order_relaxed);
      ampOnSm.skip(numSamples);
      tubeOnSm.skip(numSamples);
      // Advance EQ anyway so it doesn't "snap" when turning on
      bassDbSm.skip(numSamples);
      midDbSm.skip(numSamples);
      trebleDbSm.skip(numSamples);
    }
  }

  float getSaturationLevel() const noexcept {
    return visualLevel.load(std::memory_order_relaxed);
  }

private:
  void updateFilters() {
    float b = bassDbSm.getCurrentValue();
    float m = midDbSm.getCurrentValue();
    float t = trebleDbSm.getCurrentValue();
    float sMix = slapMixSm.getCurrentValue();

    // Smoothed Slap additions (+5dB Bass/Treble when on)
    float finalBassDb = b + (sMix * 5.0f);
    *bass.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, 85.0, 0.7, juce::Decibels::decibelsToGain(finalBassDb));

    *mid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 550.0, 0.7, juce::Decibels::decibelsToGain(m));

    // Slap Scoop: Keep filter active at target setting, mix handled in
    // process()
    *slap.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 500.0f, 1.0f, juce::Decibels::decibelsToGain(-12.0f));

    float finalTrebleDb = t + (sMix * 5.0f);
    *treble.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, 4000.0, 0.7, juce::Decibels::decibelsToGain(finalTrebleDb));

    lastB = b;
    lastM = m;
    lastT = t;
    lastSlapMix = sMix;
  }

  using IIRFilter =
      juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                     juce::dsp::IIR::Coefficients<float>>;
  IIRFilter bass, mid, treble, slap;
  IIRFilter dcBlocker;
  std::atomic<float> visualLevel{0.0f};

  // Pre-Saturation Filters (HPF + Presence Boost)
  IIRFilter preHPF;
  IIRFilter prePresence;

  // De-Saturation Filters (LPF + optional Harsh Dip)
  IIRFilter deLPF;
  IIRFilter deHarshDip;

  // Sag Module
  SagModule sagModule;

  // Moose Preamp State
  float dcFilter[2] = {0.0f, 0.0f}; // Per-channel DC removal state
  float dcRemovalCoeff = 0.0f;

  double sampleRate{44100.0};
  bool prepared{false};

  // Cache for filter updates
  float lastB{-999.0f}, lastM{-999.0f}, lastT{-999.0f};
  float lastSlapMix{-1.0f};

  // Parameters
  bool slapOn = false;
  bool autoGainEnabled = false;
  bool harshDipEnabled = false; // Optional 3.5 kHz dip
  float saturationDrive = 1.8f; // 1.0 bis 2.0 (default 1.8 for more bite)
  float inputGainDb = 0.0f;     // -6 bis +12 dB (default: 0dB)
  juce::AudioBuffer<float> scratchBuffer;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSm{
      1.0f};
  juce::SmoothedValue<float> ampOnSm;
  juce::SmoothedValue<float> tubeOnSm;
  juce::SmoothedValue<float> slapMixSm;

  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bassDbSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> midDbSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trebleDbSm{
      0.0f};
};
