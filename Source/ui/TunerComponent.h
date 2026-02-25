#pragma once
#include "../JuceIncludes.h"
#include "../dsp/AudioFifo.h"
#include "../dsp/TunerMath.h"
#include "../dsp/YinPitch.h"

class TunerComponent : public juce::Component, private juce::Timer {
public:
  TunerComponent(AudioFifo &fifoToUse, std::atomic<bool> &onFlag)
      : fifo(fifoToUse), tunerOn(onFlag) {
    setOpaque(false);
  }

  void prepare(double sampleRate) {
    sr = sampleRate;
    frameSize = (sr < 48000.0) ? 4096 : 8192; // Adaptive Frame Size
    yin.prepare(sr, frameSize);
    temp.resize((size_t)frameSize, 0.0f);
    startTimerHz(25);
  }

  ~TunerComponent() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    auto r = getLocalBounds().toFloat();

    // 1. Housing (Matching VU Meter Style)
    g.setColour(juce::Colour(0xff151515));
    g.fillRoundedRectangle(r, 4.0f);

    // Inner shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r, 4.0f, 1.5f);

    // Glass reflection
    juce::ColourGradient glassG(juce::Colours::white.withAlpha(0.05f), r.getX(),
                                r.getY(), juce::Colours::transparentWhite,
                                r.getX(), r.getBottom(), false);
    g.setGradientFill(glassG);
    g.fillRoundedRectangle(r, 4.0f);

    if (!note.valid) {
      g.setColour(juce::Colours::white.withAlpha(0.3f));
      g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
      g.drawFittedText("- - -", getLocalBounds(), juce::Justification::centred,
                       1);
      return;
    }

    // 2. Note Display (Left side)
    auto noteArea = r.removeFromLeft(60.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText(note.name, noteArea.withHeight(r.getHeight()),
               juce::Justification::centred);

    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText(juce::String(note.octave),
               noteArea.withBottom(r.getBottom() - 4.0f),
               juce::Justification::centredBottom);

    // 3. Cents Meter (Rest of the area)
    auto meterArea = r.reduced(10.0f, 6.0f);

    // Draw background ticks
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    const int numTicks = 21;
    for (int i = 0; i < numTicks; ++i) {
      float x = meterArea.getX() +
                (float)i * (meterArea.getWidth() / (float)(numTicks - 1));
      float h = (i == 0 || i == 10 || i == 20) ? 8.0f : 4.0f;
      g.drawLine(x, meterArea.getCentreY() - h, x, meterArea.getCentreY() + h,
                 1.0f);
    }

    // Center Lock LED (Green when |cents| < 3)
    float centerX = meterArea.getCentreX();
    float centerY = meterArea.getCentreY();
    bool inTune = std::abs(note.cents) <= 3;

    if (inTune) {
      // Outer glow
      g.setColour(juce::Colour(0xff00ff00).withAlpha(0.3f));
      g.fillEllipse(centerX - 16, centerY - 16, 32, 32);

      // Main LED body
      g.setColour(juce::Colour(0xff00ff00));
      g.fillEllipse(centerX - 10, centerY - 10, 20, 20);

      // Highlight (LED shine)
      g.setColour(juce::Colour(0xffffffff).withAlpha(0.6f));
      g.fillEllipse(centerX - 6, centerY - 8, 8, 8);
    }

    // The Needle/Indicator
    float needleX = juce::jmap((float)note.cents, -50.0f, 50.0f,
                               meterArea.getX(), meterArea.getRight());

    // Color gradient based on cents
    juce::Colour needleCol = juce::Colour(0xff00ffff); // Cyan (In Tune)
    if (std::abs(note.cents) > 3) {
      float factor = std::abs((float)note.cents) / 50.0f;
      needleCol = needleCol.interpolatedWith(juce::Colour(0xffff9900), factor);
    }

    g.setColour(needleCol);
    g.drawLine(needleX, meterArea.getY(), needleX, meterArea.getBottom(), 3.0f);

    // Glow for the needle
    g.setColour(needleCol.withAlpha(0.4f));
    g.drawLine(needleX, meterArea.getY(), needleX, meterArea.getBottom(), 6.0f);

    // Cents Text
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(needleCol.withAlpha(0.7f));
    g.drawText(juce::String(note.cents) + " CT",
               meterArea.removeFromTop(12).withWidth(meterArea.getWidth()),
               juce::Justification::right);
  }

private:
  void timerCallback() override {
    if (!tunerOn.load(std::memory_order_relaxed))
      return;

    // 1. CPU Guard: Nur weiter, wenn wirklich neue Daten da sind
    if (fifo.getNumReady() < frameSize)
      return;

    // 2. Daten aus der FIFO holen (wir holen das aktuellste Fenster)
    while (fifo.getNumReady() >= frameSize)
      fifo.pull(temp.data(), frameSize);

    // 3. Level Gate: RMS berechnen um Flackern bei Stille/Rauschen zu vermeiden
    // (-60 dB)
    float sumSquares = 0.0f;
    for (int i = 0; i < frameSize; ++i)
      sumSquares += temp[i] * temp[i];

    float rms = std::sqrt(sumSquares / (float)frameSize);
    const float threshold = juce::Decibels::decibelsToGain(-60.0f);

    if (rms < threshold) {
      if (note.valid) {
        note = {};
        repaint();
      }
      return;
    }

    // 4. Analyse starten (Yin Algorithmus)
    auto res = yin.process(temp.data(), frameSize);
    if (res.valid) {
      // Mini Smoothing: stabilisiert die Nadel bei leichten Schwankungen
      if (smoothedFreq <= 0.0f)
        smoothedFreq = res.frequencyHz;
      else
        smoothedFreq = 0.8f * smoothedFreq + 0.2f * res.frequencyHz;

      note = freqToNote(smoothedFreq);
    } else {
      note = {};
      smoothedFreq = 0.0f;
    }

    repaint();
  }

  AudioFifo &fifo;
  std::atomic<bool> &tunerOn;

  double sr = 44100.0;
  int frameSize = 4096;

  YinPitch yin;
  std::vector<float> temp;
  TunerNote note;
  float smoothedFreq = 0.0f;
};
