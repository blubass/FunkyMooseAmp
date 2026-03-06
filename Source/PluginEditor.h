#pragma once

#include "JuceIncludes.h"
#include "PluginProcessor.h"

#include "Skins.h"
#include "ui/ElchComponent.h"
#include "ui/FunkyMooseLookAndFeel.h"
#include "ui/TunerComponent.h"

// CMake JUCE project: no <JuceHeader.h>. Use JuceIncludes.h.

class FunkyMooseAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer {
public:
  explicit FunkyMooseAudioProcessorEditor(FunkyMooseAudioProcessor &);
  ~FunkyMooseAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  struct HoverKnob : public juce::Slider {
    using juce::Slider::Slider;
  };

  struct SimpleVUMeter : public juce::Component {
    explicit SimpleVUMeter(FunkyMooseAudioProcessorEditor &o) : owner(o) {}

    void setGRMode(bool b) { isGR = b; }

    void setVertical(bool b) {
      isVertical = b;
      repaint();
    }

    void setLevel(float lin) {
      level = juce::jlimit(0.0f, 1.0f, lin);

      // Peak hold (increased for longer persistence)
      if (level >= peak) {
        peak = level;
        peakHoldFrames = 24; // Increased from 12
      } else {
        if (peakHoldFrames > 0)
          --peakHoldFrames;
        else
          peak *= 0.96f; // Slower decay
      }

      repaint();
    }

    void paint(juce::Graphics &g) override {
      auto r = getLocalBounds().toFloat();

      // 1. Meter Housing / Glass (Deeper & more 3D)
      juce::ColourGradient housingGrad(juce::Colour(0xff080808), r.getX(),
                                       r.getY(), juce::Colour(0xff121212),
                                       r.getX(), r.getBottom(), false);
      g.setGradientFill(housingGrad);
      g.fillRoundedRectangle(r, 4.0f);

      // --- GLASS SPECULAR REFLECTION ---
      g.setColour(juce::Colours::white.withAlpha(0.04f));
      juce::Path glassPath;
      glassPath.addTriangle(r.getX(), r.getY(), r.getRight(), r.getY(), r.getX(), r.getBottom());
      g.fillPath(glassPath);

      // Deep Inner shadow for recessed feeling
      g.setColour(juce::Colours::black.withAlpha(0.9f));
      g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.8f);

      // --- METALLIC RIM / INNER GLOW ---
      g.setColour(juce::Colours::white.withAlpha(0.12f));
      g.drawRoundedRectangle(r.reduced(1.0f), 3.5f, 0.6f);

      // 2. LED Segments
      auto inner = r.reduced(4.5f, 4.5f);

      const int numSegments = 48;
      const float gap = 0.8f;
      const float vuMapping = std::pow(
          level, isGR ? 0.35f : 0.45f); // Slightly more aggressive curve

      // --- CYBER-SUNSET MULTI-STOP PALETTE ---
      auto getSegCol = [](float pos) {
        if (pos < 0.5f)
          return juce::Colour(0xff4b0082).interpolatedWith(juce::Colour(0xffff00ff), pos * 2.0f);
        if (pos < 0.85f)
          return juce::Colour(0xffff00ff).interpolatedWith(juce::Colour(0xffffd700), (pos - 0.5f) / 0.35f);
        return juce::Colour(0xffffd700).interpolatedWith(juce::Colours::white, (pos - 0.85f) / 0.15f);
      };

      for (int i = 0; i < numSegments; ++i) {
        float pos = (float)i / (float)(numSegments - 1);
        juce::Colour segCol = getSegCol(pos);

        bool active = (pos <= vuMapping);
        if (isGR) active = ((1.0f - pos) <= (1.0f - level));

        juce::Rectangle<float> segRect;
        if (isVertical) {
          float segH = (inner.getHeight() - (float)(numSegments - 1) * gap) /
                       numSegments;
          float y = inner.getBottom() - (i + 1) * (segH + gap) + gap;
          segRect = {inner.getX(), y, inner.getWidth(), segH};
        } else {
          float segW =
              (inner.getWidth() - (float)(numSegments - 1) * gap) / numSegments;
          float x = inner.getX() + i * (segW + gap);
          segRect = {x, inner.getY(), segW, inner.getHeight()};
        }

        if (active) {
          // Main Glow Core
          g.setColour(segCol.withAlpha(0.85f));
          g.fillRect(segRect);
          
          // High-Intensity core highlight
          g.setColour(juce::Colours::white.withAlpha(0.6f + (pos * 0.4f)));
          g.fillRect(segRect.reduced(isVertical ? 2.5f : 0.5f,
                                     isVertical ? 0.5f : 2.5f));
        } else {
          float distanceFromLevel = std::abs(vuMapping - pos);
          if (distanceFromLevel < 0.15f) {
            float glowAmount = 1.0f - (distanceFromLevel / 0.15f);
            g.setColour(segCol.withAlpha(0.35f * glowAmount));
            g.fillRect(segRect);
          } else {
            g.setColour(segCol.darker(0.95f).withAlpha(0.08f));
            g.fillRect(segRect);
          }
        }
      }

      // 3. PEAK HOLD INDICATOR (Stronger & Glowing)
      if (peak > 0.001f) {
        float peakVu = std::pow(peak, isGR ? 0.35f : 0.45f);
        int peakIdx =
            juce::jlimit(0, numSegments - 1, (int)(peakVu * (numSegments - 1)));

        juce::Rectangle<float> pRect;
        if (isVertical) {
          float segH = (inner.getHeight() - (float)(numSegments - 1) * gap) /
                       numSegments;
          float y = inner.getBottom() - (peakIdx + 1) * (segH + gap) + gap;
          pRect = {inner.getX(), y, inner.getWidth(), segH};
        } else {
          float segW =
              (inner.getWidth() - (float)(numSegments - 1) * gap) / numSegments;
          float x = inner.getX() + peakIdx * (segW + gap);
          pRect = {x, inner.getY(), segW, inner.getHeight()};
        }

        // --- PEAK GLOW ---
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillRoundedRectangle(pRect.expanded(2.0f), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRoundedRectangle(pRect.expanded(4.0f), 2.0f);

        g.setColour(
            juce::Colours::white.withAlpha(0.98f)); // Screaming White peak
        g.fillRect(pRect);
      }

      // 4. Labels (Unified 3D Look)
      if (meterLabel.isNotEmpty()) {
        juce::Font f(20.0f, juce::Font::bold);
        g.setFont(f);

        // Shadow for depth
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.drawText(meterLabel, r.translated(1.5f, 1.8f).toNearestInt(),
                   juce::Justification::centred, false);

        // Emboss highlight
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawText(meterLabel, r.translated(0.0f, 1.0f).toNearestInt(),
                   juce::Justification::centred, false);

        // White face
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawText(meterLabel, r.toNearestInt(), juce::Justification::centred,
                   false);
      }

      // Draw numbering scale subtly along the bottom/right edge
      g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
      int numLabels = 5;
      for (int i = 0; i < numLabels; ++i) {
        float frac = (float)i / (float)(numLabels - 1);
        juce::String numText;
        if (isGR) {
          numText = juce::String(-(int)((1.0f - frac) * 24.0f)); // 0 to -24
        } else {
          numText = juce::String((int)(frac * 60.0f) - 60); // -60 to 0
        }

        if (isVertical) {
          float ly = inner.getBottom() - frac * inner.getHeight();
          g.setColour(juce::Colours::black.withAlpha(0.8f));
          g.drawText(numText, (int)(inner.getX()), (int)(ly - 6),
                     (int)inner.getWidth(), 12, juce::Justification::centred,
                     false);
          g.setColour(juce::Colour(0xffa0a0a0).withAlpha(0.6f));
          g.drawText(numText, (int)(inner.getX()), (int)(ly - 7),
                     (int)inner.getWidth(), 12, juce::Justification::centred,
                     false);
        } else {
          float lx = inner.getX() + frac * inner.getWidth();

          // Shift labels slightly: -60 moves a bit right, 0 moves a bit left.
          // Corrected to keep -60 OUTSIDE the bar, not on top of it.
          float xShift = (i == 0) ? 1.5f : (i == numLabels - 1 ? -3.5f : 0.0f);

          g.setColour(juce::Colours::black.withAlpha(0.8f));
          g.drawText(numText, (int)(lx - 15 + xShift), (int)(inner.getY() + 2),
                     30, 12, juce::Justification::centred, false);
          g.setColour(juce::Colour(0xffa0a0a0).withAlpha(0.6f));
          g.drawText(numText, (int)(lx - 16 + xShift), (int)(inner.getY() + 1),
                     30, 12, juce::Justification::centred, false);
        }
      }

      // 4. Glass reflection (Final Layer)
      juce::ColourGradient glassG(
          juce::Colours::white.withAlpha(0.12f), r.getX(), r.getY(),
          juce::Colours::transparentWhite, r.getX(), r.getBottom(), false);
      g.setGradientFill(glassG);
      g.fillRoundedRectangle(r, 4.0f);
    }

    float level = 0.0f;
    float peak = 0.0f;
    int peakHoldFrames = 0;
    bool isGR = false;
    bool isVertical = false;
    juce::String meterLabel;
    FunkyMooseAudioProcessorEditor &owner;
  };

  struct StatsHUD : public juce::Component {
    void paint(juce::Graphics &g) override {
      auto area = getLocalBounds().toFloat();

      // Map CPU usage to color: Yellow (low) -> Cyan (high)
      juce::Colour cpuCol =
          juce::Colour(0xfff0e040)
              .interpolatedWith(juce::Colour(0xff00ffff),
                                juce::jlimit(0.0f, 1.0f, cpuUsage * 3.0f));

      float pulse =
          0.6f +
          0.4f * std::sin((float)juce::Time::getMillisecondCounterHiRes() *
                          0.006f);
      float glowAlpha = 0.2f + 0.3f * pulse;

      // 1. Background (Darker "Screen")
      g.setColour(juce::Colours::black.withAlpha(0.7f));
      g.fillRoundedRectangle(area, 4.0f);

      // 2. Inner Glow (stays inside the box)
      for (float inset = 0.0f; inset <= 4.0f; inset += 1.2f) {
        g.setColour(cpuCol.withAlpha(glowAlpha / (inset + 1.0f)));
        g.drawRoundedRectangle(area.reduced(inset + 1.0f), 4.0f, 1.5f);
      }

      // 3. Crisp Rim (Matching CPU Intensity)
      g.setColour(cpuCol.withAlpha(0.4f));
      g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.2f);

      // 4. Text (with subtle glow)
      g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
      g.setColour(cpuCol.withAlpha(0.3f * pulse));

      juce::String text = "CPU: " + juce::String(cpuUsage * 100.0f, 1) + "%";

      g.drawText(text, area.translated(1.0f, 1.0f),
                 juce::Justification::centred);

      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.drawText(text, area, juce::Justification::centred);
    }

    void update(float cpu) {
      cpuUsage = cpu;
      repaint();
    }
    void setCPU(float cpu) {
      cpuUsage = cpu;
      repaint();
    }

    float cpuUsage = 0.0f;
  };

  struct ContentCanvas : public juce::Component {
    explicit ContentCanvas(FunkyMooseAudioProcessorEditor &o) : owner(o) {}
    void paint(juce::Graphics &g) override { owner.paintContent(g); }
    void resized() override { owner.layoutContent(); }
    FunkyMooseAudioProcessorEditor &owner;
  };

  void paintContent(juce::Graphics &);
  void layoutContent();
  struct LayoutRects;
  LayoutRects getLayout() const;

  void layoutAmp(const juce::Rectangle<float> &r);
  void layoutComp(const juce::Rectangle<float> &r);
  void layoutFx(const juce::Rectangle<float> &r);
  void layoutMaster(const juce::Rectangle<float> &r);
  void layoutElchArea(const juce::Rectangle<float> &r);
  juce::File getPresetsFolder();
  void loadFactoryPresets();
  void openIrChooser();

  struct MidiIndicator : public juce::Component {
    void setLevel(float v) {
      level = v;
      repaint();
    }
    void paint(juce::Graphics &g) override {
      auto area = getLocalBounds().toFloat();

      // Let's create a bounding box for the LED at the bottom half of the
      // component
      float ledW = 22.0f;
      float ledH = 14.0f;
      auto ledR =
          juce::Rectangle<float>((area.getWidth() - ledW) * 0.5f,
                                 area.getHeight() - ledH - 4.0f, ledW, ledH);

      // 1. Label (MIDI) on top
      g.setFont(juce::Font(10.0f, juce::Font::bold));
      g.setColour(juce::Colour(0xff44ff44)); // Bright stand-out green
      g.drawText("MIDI", area.withHeight(12).translated(0, 4),
                 juce::Justification::centred, false);

      // 2. Base/Frame for LED
      g.setColour(juce::Colours::black.withAlpha(0.8f)); // Dark background
      g.fillRoundedRectangle(ledR, 3.0f);

      // Frame (Rahmen)
      g.setColour(juce::Colours::white.withAlpha(0.2f));
      g.drawRoundedRectangle(ledR, 3.0f, 1.2f);

      // Inner shadow/emboss
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.drawRoundedRectangle(ledR.reduced(1.0f), 2.0f, 1.0f);

      // 3. LED Colors (Darker green: 0xff00bb22)
      juce::Colour baseGreen = juce::Colour(0xff00bb22);

      if (level > 0.001f) {
        // Main Glow
        g.setColour(baseGreen.withAlpha(level));
        g.fillRoundedRectangle(ledR.reduced(1.5f), 2.0f);

        // Licht-Einschuss (Bright center) = HIGHLIGHT
        g.setColour(juce::Colour(0xffddffdd).withAlpha(level * 0.95f));
        g.fillRoundedRectangle(ledR.reduced(4.0f, 3.0f), 1.0f);

        // Outer Glow
        g.setColour(juce::Colours::lime.withAlpha(level * 0.5f));
        g.drawRoundedRectangle(ledR.expanded(1.5f * level), 4.0f, 2.0f);
      } else {
        // Off State - just very dark green
        g.setColour(baseGreen.withAlpha(0.15f));
        g.fillRoundedRectangle(ledR.reduced(1.5f), 2.0f);
      }
    }
    float level = 0.0f;
  };

  void timerCallback() override;

  // Typography helpers (crisp)
  void drawLabel(juce::Graphics &g, juce::Rectangle<float> r,
                 const juce::String &text, float size,
                 juce::Justification just = juce::Justification::centred,
                 bool isTopRow = false) const;

  static constexpr int designW = 2048;
  static constexpr int designH = 1152;

  FunkyMooseAudioProcessor &processor;
  FunkyMooseLookAndFeel lookAndFeel;

  // AMP (big knobs)
  HoverKnob gainKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                     juce::Slider::NoTextBox};
  HoverKnob bassKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                     juce::Slider::NoTextBox};
  HoverKnob midKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                    juce::Slider::NoTextBox};
  HoverKnob trebleKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob volumeKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};

  juce::ToggleButton slapToggle;
  juce::ToggleButton tubeToggle;
  juce::ToggleButton lowCutToggle;
  juce::ToggleButton ampOnToggle;
  juce::ToggleButton ampAutoGainToggle{"AUTO"};

  // COMP (small knobs + ratio)
  HoverKnob compInKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob compThreshKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                           juce::Slider::NoTextBox};
  HoverKnob compMakeKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox};
  HoverKnob compAtkKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                        juce::Slider::NoTextBox};
  HoverKnob compRelKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                        juce::Slider::NoTextBox};
  HoverKnob compMixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                        juce::Slider::NoTextBox};
  juce::ComboBox ratioBox;
  // Module on/off (UI toggles; wiring to DSP can come later)
  juce::ToggleButton compOn;
  juce::ToggleButton punchButton{"PUNCH"};
  juce::ToggleButton compAutoMakeupToggle{"AUTO"};
  juce::ToggleButton octOn, envOn, phaserOn, chorusOn,
      fxParallelToggle{"PARALLEL"}; // New

  juce::ToggleButton masterOn;

  // Preset & Skin UI
  juce::TextButton presetSelector{"Default"};
  juce::TextButton savePresetButton{"SAVE"};
  juce::TextButton openFolderButton{"FOLD"};
  juce::TextButton midiLearnBtn{"MIDI LRN"};   // New
  juce::ToggleButton toggleTooltips{"Values"}; // New Toggle
  StatsHUD statsHUD;                           // New Custom HUD

  // Big meter (top)
  SimpleVUMeter inVu;
  SimpleVUMeter compGr;
  juce::ToggleButton autoGateToggle{"AUTO GATE"};

  // Bottom right: output VU + Elch
  SimpleVUMeter outVu;
  ElchComponent elch;

  // FX (small knobs)
  HoverKnob oct1Knob{juce::Slider::RotaryHorizontalVerticalDrag,
                     juce::Slider::NoTextBox};
  HoverKnob oct2Knob{juce::Slider::RotaryHorizontalVerticalDrag,
                     juce::Slider::NoTextBox};
  HoverKnob octMixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};

  HoverKnob envAtkKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob envDecKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob envRangeKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox};

  HoverKnob phRateKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob phColKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox};
  HoverKnob phMixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox};

  HoverKnob chRateKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                       juce::Slider::NoTextBox};
  HoverKnob chDepthKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                        juce::Slider::NoTextBox};
  HoverKnob chMixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox};

  // MASTER
  HoverKnob outKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                    juce::Slider::NoTextBox};
  HoverKnob mixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                    juce::Slider::NoTextBox};
  HoverKnob monoMakerKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                          juce::Slider::NoTextBox}; // New
  HoverKnob irMixKnob{juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox}; // New
  juce::ToggleButton autoGainToggle{"AUTO"};    // New
  juce::ToggleButton monoMakerToggle;
  juce::TextButton cabButton{"OFF"};

  juce::ToggleButton tunerToggle, monoInputButton;
  std::unique_ptr<TunerComponent> tunerOverlay;

  ContentCanvas content{*this};

  std::unique_ptr<juce::FileChooser> irChooser;
  juce::File lastIrDirectory;

  // Attachments
  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
  using ComboAttachment =
      juce::AudioProcessorValueTreeState::ComboBoxAttachment;

  std::unique_ptr<SliderAttachment> gainAtt, bassAtt, midAtt, trebleAtt, volAtt;
  std::unique_ptr<ButtonAttachment> slapAtt, ampOnAtt, tubeAtt, lowCutAtt;

  std::unique_ptr<ButtonAttachment> compOnAtt;
  std::unique_ptr<ButtonAttachment> punchAtt;
  std::unique_ptr<SliderAttachment> compInAtt, compThrAtt, compMkAtt,
      compAtkAtt, compRelAtt, compMixAtt;
  std::unique_ptr<ComboAttachment> compRatAtt;

  std::unique_ptr<ButtonAttachment> octOnAtt;
  std::unique_ptr<SliderAttachment> oct1Att, oct2Att, octMixAtt;

  std::unique_ptr<ButtonAttachment> envOnAtt;
  std::unique_ptr<SliderAttachment> envAtkAtt, envDecAtt, envRngAtt;

  std::unique_ptr<ButtonAttachment> phOnAtt;
  std::unique_ptr<SliderAttachment> phRateAtt, phColAtt, phMixAtt;

  std::unique_ptr<ButtonAttachment> chOnAtt, fxParallelAtt; // Added Parallel
  std::unique_ptr<SliderAttachment> chRateAtt, chDepthAtt, chMixAtt;

  std::unique_ptr<SliderAttachment> outAtt, mixAtt, monoMakerAtt, irMixAtt;
  std::unique_ptr<ButtonAttachment> autoGainAtt, monoMakerOnAtt, masterOnAtt,
      autoGateAtt;
  std::unique_ptr<ButtonAttachment> ampAutoGainAtt, compAutoMakeupAtt,
      tunerAttachment, monoInputAtt;

  // Visuals
  Skins::Palette currentPalette;
  Skins::Palette targetPalette;
  float transitionProgress = 1.0f; // 1.0 = no transition active
  float tubeSatVisual = 0.0f;
  float smartGateVisual = 0.0f;
  float cpuUsage = 0.0f;
  int latencySamples = 0;

  // Cached/pre-rendered textures to reduce paint overhead
  juce::Image cachedPlateTexture;
  juce::Image cachedSkinOverlay;
  int cachedSkinIndex = -1;

  // Cached Background for ContentCanvas (Static Layout)
  juce::Image cachedContentBackground;
  void updateStaticBackground();

  // Glow Cache (Pre-rendered soft shapes)
  juce::Image cachedFxGlow;
  juce::Image cachedMasterGlow;
  void updateGlowCaches();

  // Ensure heavy-to-render textures are prepared (lazy)
  void ensureCachedTextures(int skinIndex);

  juce::ImageComponent overlayComp;
  MidiIndicator midiIndicator;
  float midiActivityIndicatorLevel = 0.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseAudioProcessorEditor)
};
