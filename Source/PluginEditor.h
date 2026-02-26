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

    void setLevel(float lin) {
      level = juce::jlimit(0.0f, 1.0f, lin);

      // Peak hold (approx. 30 Hz updates from editor timer)
      if (level >= peak) {
        peak = level;
        peakHoldFrames = 30; // ~1s
      } else {
        if (peakHoldFrames > 0)
          --peakHoldFrames;
        else
          peak *= 0.998f;
      }

      repaint();
    }

    void paint(juce::Graphics &g) override {
      auto r = getLocalBounds().toFloat();

      // 1. Meter Housing / Glass (Dark & Recessed)
      g.setColour(juce::Colour(0xff151515));
      g.fillRoundedRectangle(r, 4.0f);

      // Inner shadow for depth
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.drawRoundedRectangle(r, 4.0f, 1.5f);

      // Glass reflection
      juce::ColourGradient glassG(
          juce::Colours::white.withAlpha(0.08f), r.getX(), r.getY(),
          juce::Colours::transparentWhite, r.getX(), r.getBottom(), false);
      g.setGradientFill(glassG);
      g.fillRoundedRectangle(r, 4.0f);

      // 2. LED Segments
      auto inner = r.reduced(4.0f, 4.0f); // Padding inside glass

      // High-Res Metering (User Request: "Feinere Staffelung")
      const int numSegments = 48;
      const float gap = 0.8f;
      const float segWidth =
          (inner.getWidth() - (float)(numSegments - 1) * gap) /
          (float)numSegments;

      // Map linear -> Log/VU curve
      const float vu = std::pow(level, 0.5f); // 0.0 -> 1.0

      // Colors - "Elch Glow" (Orange -> Cyan) as requested
      const juce::Colour cOrange =
          juce::Colour(0xffff9900);                        // Warm Orange / Gold
      const juce::Colour cCyan = juce::Colour(0xff00ffff); // Electric Cyan

      for (int i = 0; i < numSegments; ++i) {
        float pos = (float)i / (float)(numSegments - 1); // 0.0 to 1.0

        // Smooth interpolation from Orange to Cyan
        // Using pos^1.5 or pos^2 to keep more orange in the lower range
        juce::Colour segCol =
            cOrange.interpolatedWith(cCyan, std::pow(pos, 1.5f));

        // Is this segment active?
        bool active = (pos <= vu);

        float x = inner.getX() + i * (segWidth + gap);
        juce::Rectangle<float> segRect(x, inner.getY(), segWidth,
                                       inner.getHeight());

        if (active) {
          // ACTIVE: Bright & Glowing
          g.setColour(segCol);
          g.fillRect(segRect);

          // Core of LED (Brighter)
          g.setColour(juce::Colours::white.withAlpha(0.4f));
          g.fillRect(segRect.reduced(0.5f, 1.0f));

        } else {
          // Subtle afterglow for segments just below current level
          // (micro-animation)
          float distanceFromLevel = vu - pos;
          if (distanceFromLevel > 0.0f && distanceFromLevel < 0.15f) {
            // Segments just behind the level get a fading glow
            float glowAmount = 1.0f - (distanceFromLevel / 0.15f);
            g.setColour(segCol.withAlpha(0.3f * glowAmount));
            g.fillRect(segRect);
          } else {
            // INACTIVE: Dark "Ghost" LED
            g.setColour(segCol.darker(0.8f).withAlpha(0.2f));
            g.fillRect(segRect);
          }
        }
      }

      // Peak Hold Indicator (White segment)
      if (peak > 0.001f) {
        float peakVu = std::pow(peak, 0.5f);
        int peakIdx = (int)(peakVu * (numSegments - 1));
        peakIdx = juce::jlimit(0, numSegments - 1, peakIdx);

        float px = inner.getX() + peakIdx * (segWidth + gap);
        juce::Rectangle<float> pRect(px, inner.getY(), segWidth,
                                     inner.getHeight());

        g.setColour(juce::Colours::white.withAlpha(0.95f)); // White peak
        g.fillRect(pRect);
      }
    }

    float level = 0.0f;
    float peak = 0.0f;
    int peakHoldFrames = 0;
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

      juce::String text = "CPU: " + juce::String(cpuUsage * 100.0f, 1) +
                          "%  LAT: " + juce::String(latencySamples);

      g.drawText(text, area.translated(1.0f, 1.0f),
                 juce::Justification::centred);

      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.drawText(text, area, juce::Justification::centred);
    }

    void update(float cpu, int lat) {
      cpuUsage = cpu;
      latencySamples = lat;
      repaint();
    }

    float cpuUsage = 0.0f;
    int latencySamples = 0;
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

  void openIrChooser();

  void timerCallback() override;

  // Typography helpers (crisp)
  void drawLabel(juce::Graphics &g, juce::Rectangle<float> r,
                 const juce::String &text, float size,
                 juce::Justification just = juce::Justification::centred) const;

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
  juce::ComboBox ratioBox;
  // Module on/off (UI toggles; wiring to DSP can come later)
  juce::ToggleButton compOn;
  juce::ToggleButton punchButton{"PUNCH"};
  juce::ToggleButton compAutoMakeupToggle{"AUTO"};
  juce::ToggleButton octOn, envOn, phaserOn, chorusOn,
      octModernToggle{"MODERN"}, fxParallelToggle{"PARALLEL"}; // New
  juce::ToggleButton masterOn;

  // Preset & Skin UI
  juce::TextButton presetSelector{"Presets..."};
  juce::TextButton savePresetButton{"SAVE"};
  juce::TextButton openFolderButton{"FOLD"};
  juce::ToggleButton toggleTooltips{"Values"}; // New Toggle
  StatsHUD statsHUD;                           // New Custom HUD

  // Big meter (top)
  SimpleVUMeter inVu;
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

  juce::ToggleButton tunerToggle;
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
      compAtkAtt, compRelAtt;
  std::unique_ptr<ComboAttachment> compRatAtt;

  std::unique_ptr<ButtonAttachment> octOnAtt, octModernAtt;
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
      tunerAttachment;

  // Visuals
  Skins::Palette currentPalette;
  Skins::Palette targetPalette;
  float transitionProgress = 1.0f; // 1.0 = no transition active
  float tubeSatVisual = 0.0f;
  float smartGateVisual = 0.0f;
  float cpuUsage = 0.0f;
  int latencySamples = 0;
  juce::ImageComponent overlayComp;

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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseAudioProcessorEditor)
};
