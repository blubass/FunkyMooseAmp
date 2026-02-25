#pragma once
#include "../JuceIncludes.h"

// Ultra-clean transparent bass compressor (JUCE dsp::ProcessorChain compatible)
// v3: LevelMatch is computed per BLOCK (smoothed), not per sample -> no digital zipper/overshoot.
// Clean goal: no coloration. Only dynamics control + safe gain staging.

class CompressorModule
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;

        scHPF.reset();
        scHPF.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 90.0f));

        setAttackMs  (attackMs);
        setReleaseMs (releaseMs);

        scEnv = 0.0f;
        gr = 1.0f;
        grDbMeter.store (0.0f);

        levelMatchGainLin = 1.0f;
    }

    void reset()
    {
        scEnv = 0.0f;
        gr = 1.0f;
        scHPF.reset();
        grDbMeter.store (0.0f);

        levelMatchGainLin = 1.0f;
    }

    // Parameters (existing mapping compatibility)
    void setCompOn (bool b)            { compOn = b; }
    void setThresholdDb (float db)     { thresholdDb = db; }

    void setRatioIndex (int idx)
    {
        static const float ratios[4] = { 4.0f, 8.0f, 12.0f, 20.0f };
        ratio = ratios[juce::jlimit (0, 3, idx)];
    }

    void setAttackMs (float ms)
    {
        attackMs = juce::jmax (0.5f, ms);
        attackCoeff = std::exp (-1.0 / (sr * (attackMs * 0.001)));
    }

    void setReleaseMs (float ms)
    {
        releaseMs = juce::jmax (10.0f, ms);
        releaseCoeff = std::exp (-1.0 / (sr * (releaseMs * 0.001)));
    }

    void setMakeupGainDb (float db)    { makeupDb = db; }

    // Recycled: old "AutoMakeup" boolean now toggles LevelMatch on/off (no UI changes).
    void setAutoMakeup (bool b)        { levelMatchEnabled = b; }

    // Optional (future): allow a continuous amount without adding UI
    void setLevelMatchAmount01 (float a01) { levelMatchAmount = juce::jlimit (0.0f, 1.0f, a01); }

    float getGainReductionDb() const   { return grDbMeter.load(); }

    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        if (context.isBypassed || ! compOn)
        {
            grDbMeter.store (0.0f);
            return;
        }

        auto block = context.getOutputBlock();
        const int numSamples = (int) block.getNumSamples();
        const int numCh      = (int) block.getNumChannels();
        if (numSamples <= 0 || numCh <= 0)
            return;

        // Manual makeup bounded (keeps it sane even if you crank the knob)
        const float manualDb = juce::jlimit (-12.0f, 6.0f, makeupDb);
        const float manualLin = juce::Decibels::decibelsToGain (manualDb);

        float meterGrDb = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            // Detector: abs max over channels (mono-safe)
            float sc = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sc = juce::jmax (sc, std::abs (block.getChannelPointer ((size_t) ch)[n]));

            // Sidechain HPF prevents low-end pumping
            sc = scHPF.processSingleSampleRaw (sc);

            // RMS-ish energy envelope
            const float target = sc * sc;
            const float c = (target > scEnv) ? (float) attackCoeff : (float) releaseCoeff;
            scEnv = c * scEnv + (1.0f - c) * target;

            const float level = std::sqrt (juce::jmax (0.0f, scEnv));
            const float inDb   = juce::Decibels::gainToDecibels (juce::jmax (level, 1.0e-9f));
            const float overDb = inDb - thresholdDb;

            float gainDb = 0.0f;
            if (overDb > 0.0f)
                gainDb = (1.0f - (1.0f / ratio)) * (-overDb);

            const float g = juce::Decibels::decibelsToGain (gainDb);

            // Smooth GR changes (fast clamp, slower release)
            const float gc = (g < gr) ? (float) std::exp (-1.0 / (sr * 0.002))
                                      : (float) std::exp (-1.0 / (sr * 0.030));
            gr = gc * gr + (1.0f - gc) * g;

            const float grDb = juce::Decibels::gainToDecibels (juce::jmax (gr, 1.0e-9f));
            meterGrDb = 0.995f * meterGrDb + 0.005f * grDb;

            // Apply ONLY GR + manual makeup here (no per-sample levelmatch anymore)
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* ptr = block.getChannelPointer ((size_t) ch);
                ptr[n] = ptr[n] * gr * manualLin;
            }
        }

        // --- LevelMatch (BLOCK based, smoothed) ---
        if (levelMatchEnabled)
        {
            // Convert average GR into a gentle compensation. Amount is 0..1 (default 0.7)
            const float amt = juce::jlimit (0.0f, 1.0f, levelMatchAmount);
            const float targetDb = juce::jlimit (0.0f, 6.0f, -meterGrDb * amt);

            const float targetLin = juce::Decibels::decibelsToGain (targetDb);

            // Smooth per-block changes to avoid zipper / overshoot
            // timeConstant ~ 150ms
            const double tc = 0.150;
            const float alpha = (float) std::exp (-((double) numSamples / sr) / tc); // 0..1
            levelMatchGainLin = alpha * levelMatchGainLin + (1.0f - alpha) * targetLin;

            block.multiplyBy (levelMatchGainLin);
        }
        else
        {
            // relax back to unity smoothly
            const double tc = 0.150;
            const float alpha = (float) std::exp (-((double) numSamples / sr) / tc);
            levelMatchGainLin = alpha * levelMatchGainLin + (1.0f - alpha) * 1.0f;
        }

        // Transparent safety ceiling (very rare if gain staging is sane)
        // Soft clipper (tanh) avoids "digital crack" if it ever hits.
        const float ceilingLin = juce::Decibels::decibelsToGain (-1.0f);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* ptr = block.getChannelPointer ((size_t) ch);
            for (int n = 0; n < numSamples; ++n)
                ptr[n] = softCeiling (ptr[n], ceilingLin);
        }

        grDbMeter.store (meterGrDb);
    }

private:
    static inline float softCeiling (float x, float ceilingLin) noexcept
    {
        const float ax = std::abs (x);
        if (ax <= ceilingLin)
            return x;

        // gentle soft clip, scaled to ceiling
        const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
        const float over = (ax - ceilingLin) / ceilingLin;   // 0..
        const float shaped = ceilingLin * (1.0f + 0.25f * std::tanh (over * 1.5f));
        return sign * shaped;
    }

    double sr = 44100.0;

    bool compOn = true;

    float thresholdDb = -18.0f;
    float ratio = 4.0f;

    float attackMs = 25.0f;
    float releaseMs = 140.0f;
    float makeupDb = 0.0f;

    double attackCoeff = 0.999;
    double releaseCoeff = 0.9999;

    float scEnv = 0.0f;
    float gr = 1.0f;

    // LevelMatch control (recycled from old AutoMakeup)
    bool  levelMatchEnabled = true;
    float levelMatchAmount = 0.70f;   // default: 70% compensation

    float levelMatchGainLin = 1.0f;

    juce::IIRFilter scHPF;
    std::atomic<float> grDbMeter { 0.0f };
};
