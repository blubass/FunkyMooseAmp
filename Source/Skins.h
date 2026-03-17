#pragma once
#include "JuceIncludes.h"

namespace Skins {
enum class FrameStyle { Rounded, Industrial, Jagged, Piping, Neon };

struct Palette {
  juce::Colour background;
  juce::Colour plate;
  juce::Colour panel;
  juce::Colour knob;
  juce::Colour knobIndicator;
  juce::Colour labelText;
  juce::Colour accent;
  juce::Colour secondaryAccent;
  FrameStyle frameStyle = FrameStyle::Rounded;
  float cornerRadius = 15.0f;
  float frameWidth = 2.0f;
  juce::Colour elchEye;
  juce::Colour elchGlow;
};

static Palette getPalette(int index) {
  switch (index) {
  case 1: // Midnight
    return {juce::Colours::black,
            juce::Colour(0xff0a0a0f),
            juce::Colour(0xff151520),
            juce::Colour(0xff202030),
            juce::Colours::cyan,
            juce::Colours::white,
            juce::Colours::cyan,
            juce::Colours::cyan.withAlpha(0.3f),
            FrameStyle::Neon,
            20.0f,
            1.0f,
            juce::Colours::cyan,
            juce::Colours::cyan.withAlpha(0.5f)};
  case 2:                              // Vintage (Gold/Bronze)
    return {juce::Colour(0xff1a1614),  // Dark bronze/black background
            juce::Colour(0xff2b2622),  // Dark textured metal plate
            juce::Colour(0xff1e1a17),  // Inset panel background
            juce::Colour(0xff1a1a1a),  // Dark Knob Base
            juce::Colour(0xffffd54f),  // Bright Gold Indicator
            juce::Colour(0xffd7ccc8),  // Light Text (off-white/beige)
            juce::Colour(0xffffb300),  // Antique Gold Accent
            juce::Colour(0xff8d6e63),  // Bronze Secondary
            FrameStyle::Piping,        // Piping style
            14.0f,                     // Corner radius
            4.0f,                      // Frame width
            juce::Colour(0xffffb300),  // Golden Eye
            juce::Colour(0x44ffb300)}; // Warm Glow
  case 3:                              // Electric
    return {juce::Colour(0xff1a237e),
            juce::Colour(0xff000000),
            juce::Colour(0xff212121),
            juce::Colour(0xff311b92),
            juce::Colours::lime,
            juce::Colours::white,
            juce::Colours::lime,
            juce::Colours::magenta,
            FrameStyle::Neon,
            10.0f,
            2.0f,
            juce::Colours::lime,
            juce::Colours::lime.withAlpha(0.6f)};
  case 4: // Used Up
    return {juce::Colour(0xff263238),
            juce::Colour(0xff37474f),
            juce::Colour(0xff455a64),
            juce::Colour(0xff757575),
            juce::Colour(0xffe0e0e0),
            juce::Colour(0xffbdbdbd),
            juce::Colour(0xff607d8b),
            juce::Colours::black,
            FrameStyle::Industrial,
            5.0f,
            2.5f,
            juce::Colours::white.withAlpha(0.7f),
            juce::Colours::grey.withAlpha(0.3f)};
  case 5: // Bloody
    return {juce::Colour(0xff000000),
            juce::Colour(0xff1a1a1a),
            juce::Colour(0xff2a0000),
            juce::Colour(0xff4a0000),
            juce::Colour(0xffff0000),
            juce::Colours::white,
            juce::Colour(0xff880000),
            juce::Colour(0xff440000),
            FrameStyle::Jagged,
            0.0f,
            2.0f,
            juce::Colours::red,
            juce::Colour(0xff880000).withAlpha(0.7f)};
  case 6: // Orange
    return {juce::Colour(0xffef6c00),
            juce::Colours::white,
            juce::Colour(0xfff5f5f5),
            juce::Colour(0xff212121),
            juce::Colours::white,
            juce::Colour(0xff212121),
            juce::Colour(0xffef6c00),
            juce::Colours::black,
            FrameStyle::Rounded,
            15.0f,
            6.0f,
            juce::Colours::orange,
            juce::Colours::orange.withAlpha(0.4f)};
  case 7:                             // Ampeg Black
    return {juce::Colour(0xff050505), // Deep black background
            juce::Colour(0xff121212), // Black textured metal plate
            juce::Colour(0xff0a0a0a), // Darker inset panel
            juce::Colour(0xfff0f0f0), // Silver/White Ampeg style knobs
            juce::Colour(0xff3f51b5), // Blue Indicator
            juce::Colour(0xffe0e0e0), // Silver/Off-white text
            juce::Colour(0xff2196f3), // Ampeg Blue Accent
            juce::Colour(0xff3f51b5), // Darker Blue
            FrameStyle::Piping,
            12.0f,
            2.0f,
            juce::Colour(0xff2196f3),
            juce::Colour(0xff3f51b5).withAlpha(0.4f)};
  case 8: // Toxic
    return {juce::Colour(0xff1b5e20),
            juce::Colour(0xff000000),
            juce::Colour(0xff212121),
            juce::Colour(0xff64dd17),
            juce::Colour(0xff000000),
            juce::Colour(0xff64dd17),
            juce::Colour(0xff64dd17),
            juce::Colours::lime,
            FrameStyle::Neon,
            30.0f,
            1.5f,
            juce::Colour(0xff64dd17),
            juce::Colours::lime.withAlpha(0.7f)};
  case 9: // CyberMoose (Neon synthwave)
    return {juce::Colour(0xff050010),
            juce::Colour(0xff1a0033),
            juce::Colour(0xff0d001a),
            juce::Colour(0xff2d004d),
            juce::Colour(0xffff00ff), // Neon Pink
            juce::Colours::white,
            juce::Colour(0xff00ffff), // Cyan
            juce::Colour(0xffff00ff).withAlpha(0.4f),
            FrameStyle::Neon,
            0.0f,
            2.0f,
            juce::Colour(0xffff00ff),
            juce::Colour(0xff00ffff).withAlpha(0.6f)};
  case 10: // IceMoose (Frozen/Crystal)
    return {juce::Colour(0xffe1f5fe),
            juce::Colour(0xffb3e5fc),
            juce::Colour(0xff81d4fa),
            juce::Colour(0xffffffff),
            juce::Colour(0xff01579b),
            juce::Colour(0xff006064),
            juce::Colour(0xff03a9f4),
            juce::Colour(0xffffffff),
            FrameStyle::Neon,
            25.0f,
            1.5f,
            juce::Colour(0xff03a9f4),
            juce::Colours::white.withAlpha(0.8f)};
  case 11: // DesertMoose (Rusty/Industrial Sand)
    return {juce::Colour(0xff3e2723),
            juce::Colour(0xff5d4037),
            juce::Colour(0xff4e342e),
            juce::Colour(0xff3e2723),
            juce::Colour(0xffff5722), // Rusty Orange
            juce::Colour(0xffffe0b2),
            juce::Colour(0xffffccbc),
            juce::Colour(0xff212121),
            FrameStyle::Jagged,
            2.0f,
            3.0f,
            juce::Colour(0xffff5722),
            juce::Colour(0xff795548).withAlpha(0.4f)};
  default: // Classic (Ampeg Black)
    return {juce::Colour(0xff050505),
            juce::Colour(0xff121212),
            juce::Colour(0xff0a0a0a),
            juce::Colour(0xfff0f0f0),
            juce::Colour(0xff3f51b5),
            juce::Colour(0xffe0e0e0),
            juce::Colour(0xff2196f3),
            juce::Colour(0xff3f51b5),
            FrameStyle::Piping,
            12.0f,
            4.0f,
            juce::Colour(0xff2196f3),
            juce::Colour(0xff3f51b5).withAlpha(0.4f)};
  }
}
} // namespace Skins
