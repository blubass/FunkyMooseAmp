#pragma once
#include "../JuceIncludes.h"

// Ultra-clean transparent bass compressor (JUCE dsp::ProcessorChain compatible)
// v4: Added "Punch" enhancement (Presence + Low-End Grip)
// Clean goal: no coloration. Only dynamics control + safe gain staging.

class CompressorModule {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sr = spec.sampleRate;

    scHPF.reset();
    scHPF.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 90.0f));

    setAttackMs(attackMs);
    setReleaseMs(releaseMs);

    scEnv = 0.0f;
    gr = 1.0f;
    grDbMeter.store(0.0f);

    levelMatchGainLin = 1.0f;

    // Timing constants for gain smoothing (gr)
    gcAttack = (float)std::exp(-1.0 / (sr * 0.002));
    gcRelease = (float)std::exp(-1.0 / (sr * 0.030));

    // Punch Filters
    punchLowShelf.prepare(spec);
    punchPresence.prepare(spec);
    updatePunchFilters();

    prepared = true;
  }

  void reset() {
    scEnv = 0.0f;
    gr = 1.0f;
    scHPF.reset();
    grDbMeter.store(0.0f);
    levelMatchGainLin = 1.0f;
    punchLowShelf.reset();
    punchPresence.reset();
  }

  // Parameters (existing mapping compatibility)
  void setCompOn(bool b) { compOn = b; }
  void setThresholdDb(float db) {
    thresholdDb = db;
    thresholdLin = juce::Decibels::decibelsToGain(db);
  }

  void setRatioIndex(int idx) {
    static const float ratios[4] = {4.0f, 8.0f, 12.0f, 20.0f};
    ratio = ratios[juce::jlimit(0, 3, idx)];
    slope = 1.0f - (1.0f / ratio);
  }

  void setAttackMs(float ms) {
    attackMs = juce::jmax(0.5f, ms);
    attackCoeff = std::exp(-1.0 / (sr * (attackMs * 0.001)));
  }

  void setReleaseMs(float ms) {
    releaseMs = juce::jmax(10.0f, ms);
    releaseCoeff = std::exp(-1.0 / (sr * (releaseMs * 0.001)));
  }

  void setMakeupGainDb(float db) {
    makeupDb = db;
    makeupLin =
        juce::Decibels::decibelsToGain(juce::jlimit(-12.0f, 30.0f, makeupDb));
  }

  void setAutoMakeup(bool b) { levelMatchEnabled = b; }

  // PUNCH enhancement
  void setPunch(bool b) noexcept {
    if (punchEnabled != b) {
      punchEnabled = b;
      updatePunchFilters();
    }
  }

  float getGainReductionDb() const { return grDbMeter.load(); }

  template <typename ProcessContext>
  void process(const ProcessContext &context) noexcept {
    if (context.isBypassed || !compOn || !prepared) {
      grDbMeter.store(0.0f);
      scEnv = 0.0f;
      gr = 1.0f;
      return;
    }

    auto block = context.getOutputBlock();
    const int numSamples = (int)block.getNumSamples();
    const int numCh = (int)block.getNumChannels();
    if (numSamples <= 0 || numCh <= 0)
      return;

    float meterGrDb = grDbMeter.load();

    for (int n = 0; n < numSamples; ++n) {
      float sc = 0.0f;
      for (int ch = 0; ch < numCh; ++ch) {
        float s = block.getSample(ch, n);
        if (!std::isfinite(s))
          s = 0.0f;
        sc = juce::jmax(sc, std::abs(s));
      }

      sc = scHPF.processSingleSampleRaw(sc);
      if (!std::isfinite(sc))
        sc = 0.0f;

      const float target = sc * sc;
      const float c =
          (target > scEnv) ? (float)attackCoeff : (float)releaseCoeff;
      scEnv = c * scEnv + (1.0f - c) * target;
      if (!std::isfinite(scEnv))
        scEnv = 0.0f;

      // Detection
      const float level = std::sqrt(juce::jmax(0.0f, scEnv));

      // Calculate Gain
      float g = 1.0f;
      if (level > thresholdLin && level > 0.00001f) {
        float inDb = juce::Decibels::gainToDecibels(level);
        float overDb = inDb - thresholdDb;
        g = juce::Decibels::decibelsToGain(-overDb * slope);
      }
      if (!std::isfinite(g))
        g = 1.0f;

      const float gc = (g < gr) ? gcAttack : gcRelease;
      gr = gc * gr + (1.0f - gc) * g;
      if (!std::isfinite(gr))
        gr = 1.0f;

      // Apply to all channels
      const float totalGain = gr * makeupLin;
      for (int ch = 0; ch < numCh; ++ch) {
        float *ptr = block.getChannelPointer((size_t)ch);
        float out = ptr[n] * totalGain;
        if (!std::isfinite(out))
          out = 0.0f;
        ptr[n] = out;
      }

      // Decimated metering
      if ((n & 31) == 0) {
        float currentGrDb = juce::Decibels::gainToDecibels(gr);
        if (!std::isfinite(currentGrDb))
          currentGrDb = 0.0f;
        meterGrDb = 0.9f * meterGrDb + 0.1f * currentGrDb;
      }
    }

    // --- PUNCH ENHANCEMENT ---
    if (punchEnabled) {
      punchLowShelf.process(context);
      punchPresence.process(context);
    }

    // --- LevelMatch ---
    if (levelMatchEnabled) {
      const float targetLin = juce::Decibels::decibelsToGain(
          juce::jlimit(0.0f, 6.0f, -meterGrDb * 0.7f));
      const float alpha = (float)std::exp(-((double)numSamples / sr) / 0.150);
      levelMatchGainLin =
          alpha * levelMatchGainLin + (1.0f - alpha) * targetLin;
      if (!std::isfinite(levelMatchGainLin))
        levelMatchGainLin = 1.0f;
      block.multiplyBy(levelMatchGainLin);
    }

    // Ceiling
    const float ceilingLin = 0.89125f; // -1dB
    for (int ch = 0; ch < numCh; ++ch) {
      float *ptr = block.getChannelPointer((size_t)ch);
      for (int n = 0; n < numSamples; ++n) {
        ptr[n] = softCeiling(ptr[n], ceilingLin);
      }
    }

    grDbMeter.store(meterGrDb);
  }

private:
  void updatePunchFilters() {
    if (sr <= 0)
      return;

    // Low-End Grip: 85Hz Low Shelf +2.5dB
    *punchLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sr, 85.0f, 0.7f, juce::Decibels::decibelsToGain(2.5f));

    // Presence: 2.8kHz Peak +2.2dB
    *punchPresence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, 2800.0f, 0.8f, juce::Decibels::decibelsToGain(2.2f));
  }

  static inline float softCeiling(float x, float ceilingLin) noexcept {
    const float ax = std::abs(x);
    if (ax <= ceilingLin)
      return x;
    const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    const float over = (ax - ceilingLin) / ceilingLin;
    const float shaped = ceilingLin * (1.0f + 0.25f * std::tanh(over * 1.5f));
    return sign * shaped;
  }

  double sr = 44100.0;
  bool compOn = true;
  float thresholdDb = -18.0f;
  float thresholdLin = 0.125f;
  float slope = 0.75f;
  float ratio = 4.0f;
  float attackMs = 25.0f;
  float releaseMs = 140.0f;
  float makeupDb = 0.0f;
  float makeupLin = 1.0f;
  double attackCoeff = 0.999;
  double releaseCoeff = 0.9999;
  float gcAttack = 0.999f;
  float gcRelease = 0.9999f;
  float scEnv = 0.0f;
  float gr = 1.0f;

  bool levelMatchEnabled = true;
  float levelMatchGainLin = 1.0f;
  bool prepared = false;

  // Punch
  bool punchEnabled = false;
  using IIRFilter =
      juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                     juce::dsp::IIR::Coefficients<float>>;
  IIRFilter punchLowShelf, punchPresence;

  juce::IIRFilter scHPF;
  std::atomic<float> grDbMeter{0.0f};
};
