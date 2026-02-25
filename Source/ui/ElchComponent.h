#pragma once
#include "JuceIncludes.h"

class ElchComponent : public juce::Component {
public:
  void setElchImage(juce::Image img) {
    elchImage = img;
    repaint();
  }
  void setGlowAmount(float g) { glowAmount = juce::jlimit(0.0f, 1.0f, g); }
  void setEyeAmount(float a) {
    eyeAmount = juce::jlimit(0.0f, 1.0f, a);
    repaint();
  }
  void setColors(juce::Colour eye, juce::Colour glow) {
    eyeColor = eye;
    glowColor = glow;
    repaint();
  }

  void setBackgroundColor(juce::Colour bg) {
    bgColor = bg;
    repaint();
  }

  void setPeakLevel(float peak) {
    // Smooth decay for natural movement
    const float targetPeak = juce::jlimit(0.0f, 1.0f, peak);
    peakLevel = peakLevel * 0.85f + targetPeak * 0.15f;
    repaint();
  }

  // Unified UI API: feed in/out RMS, compressor GR (dB) and Punch state.
  // The component will smooth internally to avoid jitter/flicker.
  void setMooseState(float inRms, float outRms, float compGRdb, bool punchOn);

  // Optional: tweak responsiveness. Defaults are musical.
  void setActivationThresholdDb(float db) { activationThresholdDb = db; }
  void setGlowPalette(juce::Colour normal, juce::Colour punch,
                      juce::Colour heavy);

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  // Optimization: Cached background (Static Elch + Vignette + Inner Shadow)
  juce::Image cachedBackground;
  void updateCachedBackground();
  // Threshold: below this output level (dBFS) the moose stays mostly idle.
  float activationThresholdDb{-50.0f};

  // Glow palette (can be overridden via setGlowPalette)
  juce::Colour glowNormal{
      juce::Colour::fromFloatRGBA(1.0f, 0.72f, 0.25f, 1.0f)}; // warm amber
  juce::Colour glowPunch{
      juce::Colour::fromFloatRGBA(0.25f, 0.85f, 1.0f, 1.0f)}; // cyan
  juce::Colour glowHeavy{
      juce::Colour::fromFloatRGBA(1.0f, 0.25f, 0.20f, 1.0f)}; // red
  float eyeFlash{0.0f};                                       // 0..1 peak flash
  juce::Colour eyeGlowColourL{
      juce::Colour::fromRGB(80, 220, 255)}; // left eye (cyan)
  juce::Colour eyeGlowColourR{
      juce::Colour::fromRGB(255, 170, 60)}; // right eye (amber)
  juce::Colour currentGlow{
      juce::Colour::fromFloatRGBA(1.0f, 0.72f, 0.25f, 1.0f)};
  juce::Image elchImage; // optional
  float glowAmount = 0.0f;
  float eyeAmount = 0.0f;
  float peakLevel = 0.0f;
  juce::Colour eyeColor = juce::Colour(0xFFFFB000);
  juce::Colour glowColor = juce::Colour(0xFFFFB000).withAlpha(0.4f);
  juce::Colour bgColor = juce::Colours::black.withAlpha(0.18f);
  float inputLevel = 0.0f;
};
