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
    // Increased frame size for better Low B stability (approx 185ms @ 44.1kHz)
    frameSize = 8192;
    yin.prepare(sr, frameSize);
    temp.resize((size_t)frameSize, 0.0f);

    // Low pass filter at 350Hz to clean up harmonics for the tuner
    const float lpfCutoff = 350.0f;
    lpfCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi *
                               lpfCutoff / (float)sr);
    lpfState = 0.0f;

    startTimerHz(25);
  }

  ~TunerComponent() override { stopTimer(); }

  void paint(juce::Graphics &g) override {
    auto r = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xff151515));
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r, 4.0f, 1.5f);

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

    auto meterArea = r.reduced(10.0f, 6.0f);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    const int numTicks = 21;
    for (int i = 0; i < numTicks; ++i) {
      float x = meterArea.getX() +
                (float)i * (meterArea.getWidth() / (float)(numTicks - 1));
      float h = (i == 0 || i == 10 || i == 20) ? 8.0f : 4.0f;
      g.drawLine(x, meterArea.getCentreY() - h, x, meterArea.getCentreY() + h,
                 1.0f);
    }

    float centerX = meterArea.getCentreX();
    float centerY = meterArea.getCentreY();
    bool inTune = std::abs(note.cents) <= 3;

    if (inTune) {
      g.setColour(juce::Colour(0xff00ff00).withAlpha(0.3f));
      g.fillEllipse(centerX - 16, centerY - 16, 32, 32);
      g.setColour(juce::Colour(0xff00ff00));
      g.fillEllipse(centerX - 10, centerY - 10, 20, 20);
      g.setColour(juce::Colour(0xffffffff).withAlpha(0.6f));
      g.fillEllipse(centerX - 6, centerY - 8, 8, 8);
    }

    float needleX = juce::jmap((float)note.cents, -50.0f, 50.0f,
                               meterArea.getX(), meterArea.getRight());
    juce::Colour needleCol = juce::Colour(0xff00ffff);
    if (std::abs(note.cents) > 3) {
      float factor = std::abs((float)note.cents) / 50.0f;
      needleCol = needleCol.interpolatedWith(juce::Colour(0xffff9900), factor);
    }

    g.setColour(needleCol);
    g.drawLine(needleX, meterArea.getY(), needleX, meterArea.getBottom(), 3.0f);
    g.setColour(needleCol.withAlpha(0.4f));
    g.drawLine(needleX, meterArea.getY(), needleX, meterArea.getBottom(), 6.0f);

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

    if (fifo.getNumReady() < frameSize)
      return;

    while (fifo.getNumReady() >= frameSize)
      fifo.pull(temp.data(), frameSize);

    // Apply 350Hz LPF to clean fundamental for better bass detection
    for (int i = 0; i < frameSize; ++i) {
      lpfState += lpfCoeff * (temp[i] - lpfState);
      temp[i] = lpfState;
    }

    float sumSquares = 0.0f;
    for (int i = 0; i < frameSize; ++i)
      sumSquares += temp[i] * temp[i];

    float rms = std::sqrt(sumSquares / (float)frameSize);
    const float threshold = juce::Decibels::decibelsToGain(
        -65.0f); // More sensitive for dying notes

    if (rms < threshold) {
      if (note.valid) {
        note = {};
        repaint();
      }
      return;
    }

    auto res = yin.process(temp.data(), frameSize);
    if (res.valid) {
      if (smoothedFreq <= 0.0f)
        smoothedFreq = res.frequencyHz;
      else
        smoothedFreq =
            0.7f * smoothedFreq + 0.3f * res.frequencyHz; // Faster response

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
  int frameSize = 8192;

  YinPitch yin;
  std::vector<float> temp;
  TunerNote note;
  float smoothedFreq = 0.0f;

  // LPF State
  float lpfCoeff{0.1f};
  float lpfState{0.0f};
};
