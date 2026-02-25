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
    dcBlocker.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        spec.sampleRate, 25.0f);

    // Pre-Emphasis: HPF 30 Hz
    preHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        spec.sampleRate, 30.0f);

    // Pre-Emphasis: Presence Peak (+1.5 dB @ 2 kHz, Q=1.0, broad)
    prePresence.coefficients =
        juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            spec.sampleRate, 2000.0f, 0.8f,
            juce::Decibels::decibelsToGain(1.5f));

    // De-Emphasis: LPF 14 kHz (smooth, no digital artifacts)
    deLPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        spec.sampleRate, 14000.0f);

    // De-Emphasis: Optional Harsh Dip at 3.5 kHz (-2 dB, wide Q=0.7)
    deHarshDip.coefficients =
        juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            spec.sampleRate, 3500.0f, 0.7f,
            juce::Decibels::decibelsToGain(-2.0f));

    // DC removal filter coefficient (5 Hz, 1-pole)
    dcRemovalCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi *
                                     5.0f / (float)spec.sampleRate);

    // Sag Module (default: amount=0.08, release=120ms)
    sagModule.prepare(spec);
    sagModule.setSagAmount(0.08f);
    sagModule.setReleaseMs(120.0f);

    prepared = true;
  }

  void setAmpOn(bool shouldBeOn) noexcept { ampOn = shouldBeOn; }
  void setTubeOn(bool shouldBeOn) noexcept { tubeOn = shouldBeOn; }
  void setSlapOn(bool shouldBeOn) noexcept { slapOn = shouldBeOn; }
  void setInputGainDb(float db) noexcept {
    inputGainDb = juce::jlimit(-6.0f, 12.0f, db);
    inputGain = juce::Decibels::decibelsToGain(inputGainDb);
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
  }

  void process(const juce::dsp::ProcessContextReplacing<float> &ctx) {
    if (!prepared)
      return;

    juce::ScopedNoDenormals noDenormals;

    // 2. Amp Simulation (Tube Sat + EQ) - Only if Amp On
    if (ampOn) {
      // --- MOOSE PREAMP: Transformer Feel ---
      float peak = 0.0f;
      if (tubeOn) {
        auto &block = ctx.getOutputBlock();

        // Pre-Saturation Filters (HPF 30 Hz + Presence Peak)
        preHPF.process(ctx);
        prePresence.process(ctx);

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
          auto *d = block.getChannelPointer(ch);

          // Create per-channel DC filter state
          float channelDcFilter = dcFilter[ch];

          for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float x = d[i];

            // Apply Input Gain (before saturation, controls drive amount)
            x = x * inputGain;

            float absX = std::abs(x);
            if (absX > peak)
              peak = absX;

            // Minimal Bias (max 0.005, sehr clean)
            const float bias = 0.002f;
            x = x + bias;

            // Solid-State Soft Clip (Mark's Method)
            float y = markSoftClip(x, saturationDrive);

            // DC Removal (1-pole HPF @ 5 Hz) - PER CHANNEL
            channelDcFilter += (y - channelDcFilter) * dcRemovalCoeff;
            y = y - channelDcFilter;

            // Output Trim
            d[i] = y * 0.85f;
          }

          // Save per-channel DC filter state
          dcFilter[ch] = channelDcFilter;
        }

        // Apply Sag (power supply collapse simulation)
        sagModule.process(ctx);

        // De-Saturation Filters (LPF 14 kHz + optional Harsh Dip @ 3.5 kHz)
        deLPF.process(ctx);
        if (harshDipEnabled) {
          deHarshDip.process(ctx);
        }

        // Subsonic protection
        dcBlocker.process(ctx);
      }
      visualLevel.store(peak, std::memory_order_relaxed);

      const double sr = sampleRate;

      // Update smoothers (approximate per block)
      auto numSamples = (int)ctx.getOutputBlock().getNumSamples();
      bassDbSm.skip(numSamples);
      midDbSm.skip(numSamples);
      trebleDbSm.skip(numSamples);

      float curBass = bassDbSm.getCurrentValue();
      float curMid = midDbSm.getCurrentValue();
      float curTreble = trebleDbSm.getCurrentValue();

      // Direct dB values passed from processor
      mid.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
          sr, 750.0, 0.9, juce::Decibels::decibelsToGain(curMid));

      if (slapOn) {
        // MID SCOOP: -12dB @ 600Hz, Q=1.0
        slap.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sr, 600.0f, 1.0f, juce::Decibels::decibelsToGain(-12.0f));
      } else {
        slap.coefficients =
            juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 1000.0f);
      }

      const float finalBassDb = slapOn ? curBass + 6.0f : curBass;
      const float finalTrebleDb = slapOn ? curTreble + 6.0f : curTreble;

      bass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
          sr, 120.0, 0.7, juce::Decibels::decibelsToGain(finalBassDb));

      treble.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
          sr, 3200.0, 0.7, juce::Decibels::decibelsToGain(finalTrebleDb));

      bass.process(ctx);
      mid.process(ctx);
      treble.process(ctx);

      if (slapOn)
        slap.process(ctx);

      // Auto-Gain Compensation
      if (autoGainEnabled) {
        // Estimate total EQ boost/cut
        float eqCompensation = 0.0f;
        eqCompensation -= curBass * 0.15f;   // Bass has strong impact
        eqCompensation -= curMid * 0.1f;     // Mid has moderate impact
        eqCompensation -= curTreble * 0.08f; // Treble has less impact
        if (slapOn)
          eqCompensation += 3.0f; // Slap adds significant boost

        float compensationGain = juce::Decibels::decibelsToGain(eqCompensation);

        // Apply compensation
        for (size_t ch = 0; ch < ctx.getOutputBlock().getNumChannels(); ++ch) {
          auto *data = ctx.getOutputBlock().getChannelPointer(ch);
          for (size_t i = 0; i < ctx.getOutputBlock().getNumSamples(); ++i) {
            data[i] *= compensationGain;
          }
        }
      }
    } else {
      visualLevel.store(0.0f, std::memory_order_relaxed);
    }
  }

  float getSaturationLevel() const noexcept {
    return visualLevel.load(std::memory_order_relaxed);
  }

private:
  juce::dsp::IIR::Filter<float> bass, mid, treble, slap;
  juce::dsp::IIR::Filter<float> dcBlocker;
  std::atomic<float> visualLevel{0.0f};

  // Pre-Saturation Filters (HPF + Presence Boost)
  juce::dsp::IIR::Filter<float> preHPF;
  juce::dsp::IIR::Filter<float> prePresence;

  // De-Saturation Filters (LPF + optional Harsh Dip)
  juce::dsp::IIR::Filter<float> deLPF;
  juce::dsp::IIR::Filter<float> deHarshDip;

  // Sag Module
  SagModule sagModule;

  // Moose Preamp State
  float dcFilter[2] = {0.0f, 0.0f}; // Per-channel DC removal state
  float dcRemovalCoeff = 0.0f;

  double sampleRate{44100.0};
  bool prepared{false};

  // Parameters
  bool ampOn = false;
  bool tubeOn = false;
  bool slapOn = false;
  bool autoGainEnabled = false;
  bool harshDipEnabled = false; // Optional 3.5 kHz dip
  float saturationDrive = 1.6f; // 1.0 bis 2.0 (default: 1.6)
  float inputGainDb = 0.0f;     // -6 bis +12 dB (default: 0dB)
  float inputGain = 1.0f;       // Linear gain multiplier

  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bassDbSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> midDbSm{0.0f};
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trebleDbSm{
      0.0f};
};
