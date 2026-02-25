#pragma once
#include "../JuceIncludes.h"
#include <map>

class FunkyMooseLookAndFeel : public juce::LookAndFeel_V4 {
public:
  FunkyMooseLookAndFeel();

  // Assets (optional)
  void setMetalOverlay(juce::Image img) { metalOverlay = img; }
  void setKnobSpec(juce::Image img) { knobSpec = img; }
  void setKnobScratches(juce::Image img) { knobScratches = img; }

  // NEW: two separate knob faces (clean metal)
  void setKnobBig(juce::Image img) { knobBig = img; }
  void setKnobSmall(juce::Image img) { knobSmall = img; }

  // Theme (can be updated by skins)
  juce::Colour accentColor = juce::Colour(0xFFFFB000);
  juce::Colour knobColor = juce::Colour(0xFFE67E22);
  juce::Colour indicatorColor = juce::Colours::white;

  void setColors(juce::Colour accent, juce::Colour knob,
                 juce::Colour indicator) {
    accentColor = accent;
    knobColor = knob;
    indicatorColor = indicator;
  }

  // Overrides
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override;

  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

  // New: Popup Bubble for Knob Values
  void drawBubble(juce::Graphics &g, juce::BubbleComponent &bubble,
                  const juce::Point<float> &tip,
                  const juce::Rectangle<float> &body) override;

private:
  juce::Image metalOverlay, knobSpec, knobScratches;
  juce::Image knobBig, knobSmall;

  // Caching for performance
  struct KnobCache {
    juce::Image base; // Ring + Background
    juce::Image cap;  // Colored Cap
    juce::Colour lastCol;
  };
  std::map<juce::String, KnobCache> knobCache;

  juce::Image glowImage; // Pre-rendered soft glow
};
