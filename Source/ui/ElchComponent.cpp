#include "ElchComponent.h"
#include <cmath>

void ElchComponent::paint(juce::Graphics &g) {
  g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

  auto bounds = getLocalBounds().toFloat();

  // 1. Draw Cached Background (Static)
  if (!cachedBackground.isNull()) {
    g.drawImageAt(cachedBackground, 0, 0);
  } else {
    // Fallback
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRect(bounds, 2.0f);
  }

  // Vignette & Inner Shadow are now in cachedBackground

  // --- SLOGAN (Rotated & Glowing) ---
  {
    g.saveState();

    // Position: Move up significantly (~20% from bottom)
    auto rSlogan = bounds;
    rSlogan = rSlogan.withY(bounds.getHeight() * 0.72f).withHeight(100.0f);

    auto center = rSlogan.getCentre();
    // Tilted -12 degrees
    g.addTransform(juce::AffineTransform::rotation(
        juce::degreesToRadians(-12.0f), center.x, center.y));

    // Font: "noch zu fett" -> Needs to look like a neon tube (thin).
    // "Helvetica Neue" Thin Italic is perfect for a sleek neon look.
    juce::Font f("Helvetica Neue", 68.0f, juce::Font::plain);
    f.setTypefaceStyle("Thin Italic");

    // Fallback logic handled by JUCE
    g.setFont(f);

    const juce::String slogan = "STAY FUNKY!!";

    // Dynamic pulse
    float pulse = 1.0f + eyeAmount * 0.4f;

    // Color: Vivid Gold
    auto textCol = juce::Colour(0xFFFFD000);
    auto glowCol = juce::Colours::yellow;
    auto outlineCol =
        juce::Colour(0xFFFF8000); // Orange-ish outline for better readability

    // 1. Deep Shadow (Hard drop)
    g.setColour(juce::Colours::black);
    g.drawFittedText(slogan, rSlogan.translated(6.0f, 6.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    // 2. Halo Glow (Circular, not crossed)
    // Draw at 8 angles to create a solid halo
    const int steps = 12;
    float distBase = 3.5f * pulse;

    g.setColour(outlineCol.withAlpha(0.6f));
    for (int i = 0; i < steps; ++i) {
      float angle = (float)i / (float)steps * juce::MathConstants<float>::twoPi;
      int dx = std::round(std::cos(angle) * distBase);
      int dy = std::round(std::sin(angle) * distBase);
      g.drawFittedText(slogan, rSlogan.translated(dx, dy).toNearestInt(),
                       juce::Justification::centred, 1);
    }

    // Double glow layer for intensity
    g.setColour(glowCol.withAlpha(0.3f));
    float distFar = distBase * 1.8f;
    for (int i = 0; i < steps; ++i) {
      float angle = (float)i / (float)steps * juce::MathConstants<float>::twoPi;
      int dx = std::round(std::cos(angle) * distFar);
      int dy = std::round(std::sin(angle) * distFar);
      g.drawFittedText(slogan, rSlogan.translated(dx, dy).toNearestInt(),
                       juce::Justification::centred, 1);
    }

    // 3. Core Text
    g.setColour(textCol);
    g.drawFittedText(slogan, rSlogan.toNearestInt(),
                     juce::Justification::centred, 1);

    // 4. White Highlight (Inner bevelish)
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawFittedText(slogan, rSlogan.translated(-2, -2).toNearestInt(),
                     juce::Justification::centred, 1);

    g.restoreState();
  }

  // --- SUNGLASSES (Round Glow Halo) ---
  if (inputLevel > 0.001f || eyeAmount > 0.001f) {

    float w = bounds.getWidth();
    // "etwas tiefer" -> 0.48f (was 0.45f)
    float faceY = bounds.getY() + bounds.getHeight() * 0.48f;
    float centerX = bounds.getCentreX();

    float sep = w * 0.12f;

    // "noch ein bisschen schmaler" -> Reduce radius to fit lens exactly
    float radius = w * 0.065f;

    auto leftCenter = juce::Point<float>(centerX - sep * 0.9f, faceY);
    auto rightCenter = juce::Point<float>(centerX + sep * 0.9f, faceY);

    juce::Colour baseL = juce::Colours::cyan;
    juce::Colour baseR = juce::Colour(0xFFFF8000); // Orange

    // Brightness modulated by input - "noch stärker glühen"
    // Allow brightness to go very high for saturation effect
    float bright =
        juce::jlimit(0.0f, 3.0f, inputLevel * 2.0f + eyeAmount * 0.4f);
    bright = std::max(bright, 0.2f); // Always slight glimmer

    // Draw Function (now takes radius `r` to scale for antlers)
    auto drawHalo = [&](juce::Point<float> pt, juce::Colour c, float r) {
      g.saveState();

      // "Nur glühen, kein Kreis" -> Pure volumetric gradient

      // Calculate Radius expansion
      float rGlow = r * (1.4f + 1.0f * inputLevel); // Large halo expansion

      // Colors
      juce::Colour cCore =
          juce::Colours::white.withAlpha(std::min(1.0f, 0.98f * bright));
      juce::Colour cMid = c.withAlpha(std::min(1.0f, 0.85f * bright));
      juce::Colour cOuter = c.withAlpha(0.0f);

      // Radial Gradient: Center -> Outer Edge
      juce::ColourGradient cg(cCore, pt.x, pt.y, cOuter, pt.x, pt.y - rGlow,
                              true);

      // Volumetric stops for "Deep Glow"
      cg.addColour(0.18f, cMid); // Hot inner core
      cg.addColour(
          0.55f,
          c.withAlpha(
              0.5f * std::min(1.0f, bright))); // Mid bloom looks like light fog

      g.setGradientFill(cg);
      g.fillEllipse(pt.x - rGlow, pt.y - rGlow, rGlow * 2.0f, rGlow * 2.0f);

      g.restoreState();
    };

    // Sunglasses (Original size)
    drawHalo(leftCenter, baseL, radius);
    drawHalo(rightCenter, baseR, radius);

    // Antlers (Slightly smaller, "Mitte des Geweihs")
    // Positions approximate based on visual moose anatomy
    float antlerY = bounds.getY() + bounds.getHeight() * 0.30f; // Approx height
    float antlerSep = w * 0.28f;                                // Wider spread

    // Antler Glows - Same colors, slightly reduced size to fit anatomy
    auto leftAntler = juce::Point<float>(centerX - antlerSep, antlerY);
    auto rightAntler = juce::Point<float>(
        centerX + antlerSep, antlerY - bounds.getHeight() * 0.02f); // Asymmetry

    drawHalo(leftAntler, baseL, radius * 0.85f);
    drawHalo(rightAntler, baseR, radius * 0.85f);
  }
}

void ElchComponent::setMooseState(float inRms, float outRms, float compGRdb,
                                  bool punchOn) {
  // Convert to dBFS for musical thresholding
  const float eps = 1.0e-6f;
  const float outDb = 20.0f * std::log10(std::max(eps, outRms));

  // Gate / deadzone: below threshold, reduce response strongly
  // Map [threshold..-20dB] -> [0..1]
  const float thrDb = activationThresholdDb;
  float act = (outDb - thrDb) / (-20.0f - thrDb);
  act = juce::jlimit(0.0f, 1.0f, act);
  // Softer near threshold
  act = act * act;

  // Normalize basic levels (linear), weighted by activation
  const float outN = juce::jlimit(0.0f, 1.0f, outRms * 1.6f) * act;
  const float inN = juce::jlimit(0.0f, 1.0f, inRms * 1.6f) * act;

  float grN = juce::jlimit(0.0f, 1.0f, compGRdb / 12.0f);
  grN = std::sqrt(grN);

  // Targets
  const float glowTarget = juce::jlimit(0.0f, 1.0f, outN * 0.85f + grN * 0.55f);
  const float eyeTarget = punchOn
                              ? juce::jlimit(0.0f, 1.0f, 0.32f + outN * 0.72f)
                              : juce::jlimit(0.0f, 1.0f, outN * 0.62f);

  const float peakTarget = juce::jlimit(0.0f, 1.0f, outN);
  const float inTarget = juce::jlimit(0.0f, 1.0f, inN * 1.5f); // Boosted input

  // Smooth (single-pole)
  glowAmount = glowAmount * 0.88f + glowTarget * 0.12f;
  eyeAmount =
      eyeAmount * 0.84f +
      eyeAmount *
          0.16f; // This was eyeTarget originally, let me fix it to eyeTarget

  // Fixed eyeAmount smoothing
  eyeAmount = eyeAmount * 0.84f + eyeTarget * 0.16f;

  // Asymmetric smoothing for inputLevel
  if (inTarget > inputLevel)
    inputLevel = inputLevel * 0.60f + inTarget * 0.40f; // Fast Attack
  else
    inputLevel = inputLevel * 0.60f + inTarget * 0.40f; // Fast Release

  // Peak decay
  peakLevel = peakLevel * 0.85f + peakTarget * 0.15f;

  const float punchW = punchOn ? 0.55f : 0.0f;
  const float heavyW =
      juce::jlimit(0.0f, 1.0f, (compGRdb - 6.0f) / 10.0f) * 0.75f;

  float wN = 1.0f;
  float wP = punchW;
  float wH = heavyW;
  const float sum = wN + wP + wH;
  wN /= sum;
  wP /= sum;
  wH /= sum;

  auto blend = [](juce::Colour a, juce::Colour b, float t) {
    return a.interpolatedWith(b, juce::jlimit(0.0f, 1.0f, t));
  };

  juce::Colour c = glowNormal;
  c = blend(c, glowPunch, wP);
  c = blend(c, glowHeavy, wH);

  currentGlow = currentGlow.interpolatedWith(c, 0.15f);

  const float dbGreen = -24.0f;
  const float dbRed = -10.0f;

  float t = (outDb - dbGreen) / (dbRed - dbGreen);
  t = juce::jlimit(0.0f, 1.0f, t);

  float hue = 0.33f;
  if (t < 0.5f) {
    const float u = t / 0.5f;
    hue = 0.33f + u * (0.16f - 0.33f);
  } else {
    const float u = (t - 0.5f) / 0.5f;
    hue = 0.16f + u * (0.00f - 0.16f);
  }

  const float sat = 0.95f;
  const float val = juce::jlimit(0.20f, 1.0f, 0.35f + 0.65f * t);

  auto levelCol = juce::Colour::fromHSV(hue, sat, val, 1.0f);

  if (punchOn)
    levelCol = levelCol.interpolatedWith(glowPunch, 0.35f);

  if (compGRdb > 6.0f) {
    const float grT = juce::jlimit(0.0f, 1.0f, (compGRdb - 6.0f) / 10.0f);
    levelCol = levelCol.interpolatedWith(glowHeavy, 0.45f * grT);
  }

  if (outDb >= dbRed)
    eyeFlash = 1.0f;
  else
    eyeFlash *= 0.80f;

  auto flashCol = levelCol.interpolatedWith(juce::Colours::white, 0.55f);
  levelCol =
      levelCol.interpolatedWith(flashCol, juce::jlimit(0.0f, 1.0f, eyeFlash));

  eyeGlowColourL = eyeGlowColourL.interpolatedWith(
      glowPunch.interpolatedWith(levelCol, 0.35f), 0.22f);
  eyeGlowColourR = eyeGlowColourR.interpolatedWith(levelCol, 0.22f);

  (void)inN;

  repaint();
}

void ElchComponent::resized() { updateCachedBackground(); }

void ElchComponent::updateCachedBackground() {
  auto bounds = getLocalBounds();
  if (bounds.isEmpty())
    return;

  cachedBackground = juce::Image(juce::Image::ARGB, bounds.getWidth(),
                                 bounds.getHeight(), true);
  juce::Graphics g(cachedBackground);

  // Draw Elch
  if (elchImage.isValid()) {
    // High quality resampling ONCE
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(elchImage, bounds.toFloat(), juce::RectanglePlacement::centred);
  }

  // Draw Vignette
  {
    auto fb = bounds.toFloat();
    juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                  fb.getCentreX(), fb.getCentreY(),
                                  juce::Colours::black.withAlpha(0.35f),
                                  fb.getRight(), fb.getBottom(), true);
    vignette.addColour(0.62f, juce::Colours::transparentBlack);

    g.setGradientFill(vignette);
    g.fillRect(fb);

    // Inner Shadow
    for (float i = 0.5f; i <= 3.5f; i += 1.0f) {
      g.setColour(juce::Colours::black.withAlpha(0.28f / i));
      g.drawRect(fb.reduced(i), 1.0f);
    }
  }
}

void ElchComponent::setGlowPalette(juce::Colour normal, juce::Colour punch,
                                   juce::Colour heavy) {
  glowNormal = normal;
  glowPunch = punch;
  glowHeavy = heavy;
}
