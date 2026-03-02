#include "FunkyMooseLookAndFeel.h"

FunkyMooseLookAndFeel::FunkyMooseLookAndFeel() {
  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff121212));
  setColour(juce::PopupMenu::textColourId, juce::Colours::white);
  setColour(juce::PopupMenu::headerTextColourId,
            juce::Colours::white.withAlpha(0.6f));
  setColour(juce::PopupMenu::highlightedBackgroundColourId,
            juce::Colour(0xfff0e040).withAlpha(0.3f));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void FunkyMooseLookAndFeel::drawRotarySlider(
    juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider) {

  const juce::String id = slider.getComponentID().trim();
  bool isMaster = id.equalsIgnoreCase("MASTER");

  // Create a unique key for this knob type and size
  juce::String cacheKey =
      id + "_" + juce::String(width) + "x" + juce::String(height);
  auto &cache = knobCache[cacheKey];

  // --- Dynamic Color Logic ---
  juce::Colour baseColour = juce::Colour(0xff303030); // Default Dark
  if (id.equalsIgnoreCase("AMP"))
    baseColour = juce::Colour::fromRGB(200, 50, 50);
  else if (id.equalsIgnoreCase("COMP"))
    baseColour = juce::Colour::fromRGB(40, 90, 200);
  else if (id.equalsIgnoreCase("OCT"))
    baseColour = juce::Colour::fromRGB(220, 130, 30);
  else if (id.equalsIgnoreCase("ENV"))
    baseColour = juce::Colour::fromRGB(50, 160, 60);
  else if (id.equalsIgnoreCase("PH"))
    baseColour = juce::Colour::fromRGB(160, 60, 170);
  else if (id.equalsIgnoreCase("CH"))
    baseColour = juce::Colour::fromRGB(30, 170, 180);
  else if (isMaster)
    baseColour = juce::Colour(0xff454545);
  else if (id.isEmpty())
    baseColour = juce::Colour(0xffb4b4b4);

  // If cache is invalid, pre-render the static parts
  if (cache.base.isNull() || cache.lastCol != baseColour) {
    cache.base = juce::Image(juce::Image::ARGB, width, height, true);
    juce::Graphics ig(cache.base);

    auto bounds =
        juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(2.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();

    // --- Thin Brushed Studio Ring ---
    auto ringBounds = bounds.expanded(3.0f);
    juce::Colour ringCol = juce::Colour::fromRGB(88, 75, 62);
    juce::ColourGradient studioRingGrad(
        ringCol.brighter(0.15f), ringBounds.getCentreX(), ringBounds.getY(),
        ringCol.darker(0.1f), ringBounds.getCentreX(), ringBounds.getBottom(),
        false);
    ig.setGradientFill(studioRingGrad);
    ig.drawEllipse(ringBounds, 1.5f);

    ig.saveState();
    juce::Path clipPath;
    clipPath.addEllipse(ringBounds);
    ig.reduceClipRegion(clipPath);
    for (float yy = ringBounds.getY(); yy < ringBounds.getBottom();
         yy += 2.0f) {
      float alpha = 0.03f + 0.02f * std::sin(yy * 0.5f);
      ig.setColour(juce::Colours::white.withAlpha(alpha));
      ig.drawLine(ringBounds.getX(), yy, ringBounds.getRight(), yy, 0.8f);
    }
    ig.restoreState();

    // --- REALISTIC 3D DROP SHADOW ---
    {
      juce::Path shadow;
      shadow.addEllipse(bounds);

      // 1. Soft Ambient / Directional Shadow (Wide & Soft)
      juce::DropShadow dsAmbient(juce::Colours::black.withAlpha(0.55f), 14,
                                 {2, 4});
      dsAmbient.drawForPath(ig, shadow);

      // 2. Core Shadow (Darker, tighter, pulling down-right)
      juce::DropShadow dsCore(juce::Colours::black.withAlpha(0.75f), 8, {1, 2});
      dsCore.drawForPath(ig, shadow);

      // 3. Contact Shadow (Very tight, anchors the knob to the plate)
      juce::DropShadow dsContact(juce::Colours::black.withAlpha(0.95f), 3,
                                 {0, 1});
      dsContact.drawForPath(ig, shadow);
    }

    // --- OUTER METAL RING ---
    auto outer = bounds;
    ig.setColour(juce::Colours::black.withAlpha(isMaster ? 0.45f : 0.35f));
    ig.fillEllipse(outer.translated(1.2f, 2.2f).reduced(6.0f));

    // Core of the knob body (Always solid)
    ig.setColour(juce::Colour::fromRGB(18, 18, 18));
    ig.fillEllipse(outer);

    if (isMaster) {
      ig.setColour(juce::Colours::black.withAlpha(0.6f));
      ig.drawEllipse(outer, 1.0f);
    }

    juce::Path ring;
    ring.addEllipse(outer);
    ring.addEllipse(outer.reduced(3.0f));
    ring.setUsingNonZeroWinding(false);
    juce::ColourGradient ringGrad(juce::Colours::white.withAlpha(0.85f),
                                  outer.getX(), outer.getY(),
                                  juce::Colours::black.withAlpha(1.00f),
                                  outer.getRight(), outer.getBottom(), false);
    ig.setGradientFill(ringGrad);
    ig.fillPath(ring);
    ig.setColour(juce::Colours::black.withAlpha(0.95f));
    ig.drawEllipse(outer.reduced(3.0f), 1.0f);

    const int ticks = 36;
    auto ringR = (outer.getWidth() * 0.5f) - 2.0f;
    ig.setColour(juce::Colours::black.withAlpha(0.18f));
    for (int i = 0; i < ticks; ++i) {
      float a = juce::MathConstants<float>::twoPi * (float)i / (float)ticks;
      float x1 = cx + std::cos(a) * (ringR - 1.5f);
      float y1 = cy + std::sin(a) * (ringR - 1.5f);
      float x2 = cx + std::cos(a) * (ringR - 4.5f);
      float y2 = cy + std::sin(a) * (ringR - 4.5f);
      ig.drawLine(x1, y1, x2, y2, 1.0f);
    }

    // --- KNOB CAP ---
    auto cap = outer.reduced(4.0f);
    for (float i = 1.0f; i <= 3.0f; i += 1.0f) {
      ig.setColour(baseColour.withAlpha(0.10f / i));
      ig.drawEllipse(cap.expanded(i), 1.5f);
    }

    // Minimal shadow for Master knobs to maintain solid metallic look
    float bottomDarken =
        isMaster ? 0.95f : (id.equalsIgnoreCase("AMP") ? 0.88f : 0.85f);

    juce::ColourGradient capGrad(baseColour.brighter(0.8f), cx - radius * 0.4f,
                                 cy - radius * 0.4f,
                                 baseColour.darker(bottomDarken),
                                 cx + radius * 0.6f, cy + radius * 0.6f, true);
    ig.setGradientFill(capGrad);
    ig.fillEllipse(cap);
    ig.setColour(juce::Colours::black.withAlpha(0.85f));
    ig.drawEllipse(cap.reduced(1.0f), 1.5f);

    // Hard Bevel Highlight (Top Leftish)
    juce::ColourGradient bevelHigh(
        juce::Colours::white.withAlpha(0.85f), cx - radius * 0.7f,
        cy - radius * 0.7f, juce::Colours::transparentWhite, cx, cy, true);
    ig.setGradientFill(bevelHigh);
    ig.drawEllipse(cap.reduced(0.5f), 2.5f);

    // Hard Bevel Shadow (Bottom Rightish)
    juce::ColourGradient bevelDark(juce::Colours::transparentWhite, cx, cy,
                                   juce::Colours::black.withAlpha(1.0f),
                                   cx + radius * 0.7f, cy + radius * 0.7f,
                                   true);
    ig.setGradientFill(bevelDark);
    ig.drawEllipse(cap.reduced(0.5f), 2.5f);

    if (id.equalsIgnoreCase("AMP")) {
      ig.setColour(juce::Colours::white.withAlpha(0.18f));
      ig.drawEllipse(cap.reduced(1.4f), 1.0f);
    }
    ig.setColour(juce::Colours::black.withAlpha(
        id.equalsIgnoreCase("AMP") ? 0.75f : 0.65f));
    ig.drawEllipse(cap.reduced(2.2f), 1.0f);

    cache.lastCol = baseColour;
  }

  // --- DRAW CACHED BASE ---
  g.drawImageAt(cache.base, x, y);

  // --- DYNAMIC PARTS (Indicator, Specular, Notches) ---
  auto bounds =
      juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)
          .reduced(2.0f);
  float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
  float cx = bounds.getCentreX();
  float cy = bounds.getCentreY();
  auto cap = bounds.reduced(4.0f);

  const float myStartAngle = juce::MathConstants<float>::pi * 0.75f;
  const float myEndAngle = juce::MathConstants<float>::pi * 2.25f;
  float angle = myStartAngle + sliderPos * (myEndAngle - myStartAngle);

  // --- DULL VINTAGE SPECULAR HIGHLIGHT ---
  float hiReduce = isMaster ? 0.885f : (id == "AMP" ? 0.86f : 0.84f);
  auto hi = cap.reduced(cap.getWidth() * hiReduce);
  float specX = cx - cap.getWidth() * 0.30f + std::cos(angle) * radius * 0.12f;
  float specY = cy - cap.getHeight() * 0.30f + std::sin(angle) * radius * 0.12f;
  hi.setPosition(specX, specY);
  float highlightAlpha =
      isMaster
          ? 0.75f
          : (id == "AMP" ? 0.65f : 0.60f); // Boosted highlight for extreme 3D
  juce::Colour agedWhite(0xffebd8b8);      // Yellowed sun-faded plastic white

  juce::ColourGradient shine(
      agedWhite.withAlpha(highlightAlpha), hi.getCentreX(), hi.getCentreY(),
      juce::Colours::transparentWhite, hi.getRight(), hi.getBottom(), true);
  g.setGradientFill(shine);
  g.fillEllipse(hi);

  // Dull hot spot inside the specular
  g.setColour(
      agedWhite.withAlpha(highlightAlpha * 0.5f)); // Much softer hot spot
  g.fillEllipse(hi.reduced(hi.getWidth() * 0.4f));

  float specAlpha = isMaster ? 0.25f : 0.15f; // Dulled ring
  g.setColour(agedWhite.withAlpha(specAlpha));
  g.drawEllipse(hi.reduced(hi.getWidth() * 0.3f), 1.2f);
  float pingAlpha = isMaster ? 0.60f : 0.45f;
  float pingX = cx - radius * 0.45f + std::cos(angle - 0.3f) * radius * 0.08f;
  float pingY = cy - radius * 0.45f + std::sin(angle - 0.3f) * radius * 0.08f;
  g.setColour(agedWhite.withAlpha(pingAlpha));
  g.fillEllipse(pingX, pingY, 2.5f, 2.5f);

  // Dirt on the cap (subtle speckled grime)
  juce::Random grimeRng((int)(x + y + sliderPos * 100)); // Static per position
  g.setColour(juce::Colours::black.withAlpha(0.12f));
  for (int i = 0; i < 40; ++i) {
    float gx = cx - radius * 0.6f + grimeRng.nextFloat() * radius * 1.2f;
    float gy = cy - radius * 0.6f + grimeRng.nextFloat() * radius * 1.2f;
    g.fillEllipse(gx, gy, 1.0f + grimeRng.nextFloat() * 1.5f,
                  1.0f + grimeRng.nextFloat() * 1.5f);
  }

  // Bounce Light (Subtle reflection from bottom right)
  auto bounce = cap.reduced(cap.getWidth() * 0.85f);
  float bounceX =
      cx + cap.getWidth() * 0.35f +
      std::cos(angle + juce::MathConstants<float>::pi) * radius * 0.1f;
  float bounceY =
      cy + cap.getHeight() * 0.35f +
      std::sin(angle + juce::MathConstants<float>::pi) * radius * 0.1f;
  bounce.setPosition(bounceX, bounceY);
  juce::ColourGradient bounceGrad(juce::Colours::white.withAlpha(0.08f),
                                  bounce.getCentreX(), bounce.getCentreY(),
                                  juce::Colours::transparentWhite,
                                  bounce.getX(), bounce.getY(), true);
  g.setGradientFill(bounceGrad);
  g.fillEllipse(bounce);

  // Antler Notches (Master)
  if (radius > 26.0f) {
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    for (int side = -1; side <= 1; side += 2) {
      float a = -juce::MathConstants<float>::halfPi + side * 0.35f;
      float rr = radius * 0.92f;
      juce::Point<float> p1(cx + std::cos(a) * rr, cy + std::sin(a) * rr);
      juce::Point<float> p2(cx + std::cos(a) * (rr - 5.f),
                            cy + std::sin(a) * (rr - 5.f));
      g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f);
      g.drawLine(p2.x, p2.y, p2.x + side * 2.0f, p2.y + 2.0f, 1.4f);
    }
  }

  // Indicator (3D Needle)
  {
    float pointerLength = radius * 0.78f;
    float pointerWidth = juce::jmax(2.0f, radius * 0.095f);
    juce::Point<float> p1(cx, cy);
    juce::Point<float> p2(cx + std::cos(angle) * pointerLength,
                          cy + std::sin(angle) * pointerLength);

    // Carved Needle Shadow (Inner cut)
    g.setColour(juce::Colours::black.withAlpha(1.0f)); // Max dark
    g.drawLine(p1.x + 2.5f, p1.y + 2.5f, p2.x + 2.5f, p2.y + 2.5f,
               pointerWidth * 1.5f); // Wider trench

    // Catch light on the edge of the carved trench
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawLine(p1.x - 2.0f, p1.y - 2.0f, p2.x - 2.0f, p2.y - 2.0f,
               pointerWidth * 1.0f);

    // Needle Body (Aged off-white plastic with grime)
    juce::Colour agedNeedleWhite = juce::Colour(0xffebd8b8);
    juce::ColourGradient needleGrad(agedNeedleWhite.darker(0.1f), p1,
                                    agedNeedleWhite.darker(0.5f), p2, false);
    g.setGradientFill(needleGrad);
    g.drawLine(p1.x, p1.y, p2.x, p2.y, pointerWidth);

    // Sharp Center Ridge on needle (Dirtier)
    g.setColour(agedNeedleWhite.withAlpha(0.3f));
    g.drawLine(p1.x, p1.y, p2.x, p2.y, pointerWidth * 0.3f);
  }

  // Moose Emboss / 3D Text
  if (slider.getName() == "GAIN_AMP") {
    const float s = juce::jmax(4.25f, radius * 0.238f);
    auto m = juce::Rectangle<float>(cx - s * 0.5f, cy - s * 0.35f, s, s * 0.7f);
    juce::Path p;
    p.startNewSubPath(m.getX(), m.getBottom());
    p.lineTo(m.getX(), m.getY());
    p.lineTo(m.getCentreX(), m.getY() + m.getHeight() * 0.55f);
    p.lineTo(m.getRight(), m.getY());
    p.lineTo(m.getRight(), m.getBottom());
    juce::Path darkPath(p);
    darkPath.applyTransform(juce::AffineTransform::translation(1.0f, 1.2f));
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.strokePath(darkPath,
                 juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    juce::Path lightPath(p);
    lightPath.applyTransform(juce::AffineTransform::translation(-1.0f, -1.2f));
    g.setColour(juce::Colours::white.withAlpha(0.36f));
    g.strokePath(lightPath,
                 juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
  } else if (slider.getName() == "MASTER_OUT") {
    // 3D Embossed "MASTER" text
    juce::Font masterFont(juce::jmax(12.0f, radius * 0.35f), juce::Font::bold);
    g.setFont(masterFont);

    juce::String text = "MASTER";
    juce::Rectangle<float> textArea(cx - radius * 0.7f, cy - radius * 0.7f,
                                    radius * 1.4f, radius * 1.4f);

    // Dark inner shadow (cut-in effect)
    g.setColour(juce::Colours::black.withAlpha(1.0f));
    g.drawFittedText(text, textArea.translated(0.0f, 3.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    // Light specular highlight (bottom edge of cut)
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawFittedText(text, textArea.translated(0.0f, -2.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    // Base color or slight gradient for the text itself (to look embossed)
    g.setColour(baseColour.darker(0.3f));
    g.drawFittedText(text, textArea.toNearestInt(),
                     juce::Justification::centred, 1);
  }
}

void FunkyMooseLookAndFeel::drawToggleButton(juce::Graphics &g,
                                             juce::ToggleButton &button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown) {
  // Fix: Static size for the button itself (54x24) centered in component
  // (84x40)
  const bool on = button.getToggleState();
  const bool hover =
      button.isMouseOverOrDragging() || shouldDrawButtonAsHighlighted;
  const bool down = button.isMouseButtonDown() || shouldDrawButtonAsDown;

  auto buttonName = button.getName();
  const bool isMainToggle = (buttonName == "mainToggle");
  const bool isMono = (buttonName == "monoInput");
  const bool isValues = (buttonName == "tooltipToggle");
  const bool isTuner = (buttonName == "TUNER");

  float targetW = 54.0f;
  float targetH = 24.0f;
  if (isMono || isValues || isTuner) {
    targetW = 88.0f; // Significantly wider
    targetH = 30.0f; // Taller
  }
  auto r =
      button.getLocalBounds().toFloat().withSizeKeepingCentre(targetW, targetH);

  // --- 1. PRE-BODY GLOW (HALO) - Subtler & Tighter ---
  auto lamp = r.reduced(4.0f);
  float glowA = on ? 0.45f : (hover ? 0.12f : 0.03f);
  if (down)
    glowA *= 1.1f;

  if (on) {
    auto bloomCol =
        (isValues)
            ? juce::Colour(0xff22ff22)
            : (isMono ? juce::Colour::fromRGB(45, 120, 45)
                      : (isMainToggle ? juce::Colour::fromRGB(255, 40, 0)
                                      : juce::Colour::fromRGB(35, 95, 35)));

    // Realistic tight halo (Very steep exponential falloff)
    for (int i = 1; i <= 8; ++i) {
      float exp = 1.25f * i; // Max expansion ~10px
      float alpha = glowA * 0.45f * std::exp(-0.75f * (float)i);
      g.setColour(bloomCol.withAlpha(alpha));
      g.fillRoundedRectangle(lamp.expanded(exp), 5.0f + exp);
    }
  }

  // --- 2. THE BUTTON BODY ---
  juce::DropShadow btnShadow(juce::Colours::black.withAlpha(0.9f), 6, {0, 2});
  juce::Path btnPath;
  btnPath.addRoundedRectangle(r, 4.0f);
  btnShadow.drawForPath(g, btnPath);

  juce::ColourGradient btnGrad(juce::Colour(0xff454545), r.getCentreX(),
                               r.getY(), juce::Colour(0xff050505),
                               r.getCentreX(), r.getBottom(), false);
  g.setGradientFill(btnGrad);
  g.fillRoundedRectangle(r, 4.0f);

  if (isValues) {
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.8f);
  } else {
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(r, 4.0f, 1.8f);
  }

  // --- 3. THE LAMP (ILLUMINATED AREA) ---
  auto lampCol =
      on ? (isValues
                ? juce::Colour(0xff22ff22)
                : ((isMono || isTuner)
                       ? juce::Colour::fromRGB(65, 185, 65)
                       : (isMainToggle ? juce::Colour::fromRGB(240, 60, 0)
                                       : juce::Colour::fromRGB(50, 140, 50))))
         : (isMono ? juce::Colour::fromRGB(15, 25, 15)
                   : (isMainToggle ? juce::Colour::fromRGB(12, 12, 12)
                                   : juce::Colour::fromRGB(12, 20, 12)));

  if (on) {
    // Perfectly Symmetrical Fill with Radial Core Glow
    juce::ColourGradient coreGlow(lampCol.brighter(0.5f), lamp.getCentreX(),
                                  lamp.getCentreY(), lampCol, lamp.getCentreX(),
                                  lamp.getY(), true);
    g.setGradientFill(coreGlow);
    g.fillRoundedRectangle(lamp, 3.0f);

    // Smooth internal highlight
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(lamp.reduced(1.0f), 3.0f, 1.0f);
  } else {
    g.setColour(lampCol);
    g.fillRoundedRectangle(lamp, 3.0f);
  }

  // --- 4. CHROME REFLEX HIGHLIGHT (Premium Glassy Look) ---
  {
    auto reflexArea = lamp.reduced(0.5f);
    reflexArea.setHeight(reflexArea.getHeight() * 0.45f);

    juce::ColourGradient chromeGrad(
        juce::Colours::white.withAlpha(on ? 0.28f : 0.14f), reflexArea.getX(),
        reflexArea.getY(), juce::Colours::white.withAlpha(0.0f),
        reflexArea.getX(), reflexArea.getBottom(), false);
    g.setGradientFill(chromeGrad);
    g.fillRoundedRectangle(reflexArea, 2.5f);

    // Diagonal "Sweep" highlight for extra chrome feel
    g.setColour(juce::Colours::white.withAlpha(on ? 0.08f : 0.04f));
    juce::Path sweep;
    sweep.addRectangle(lamp.getX() + 5.0f, lamp.getY(), 12.0f,
                       lamp.getHeight());
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(0.4f, lamp.getCentreX(),
                                                   lamp.getCentreY()));
    g.reduceClipRegion(lamp.toNearestInt());
    g.fillPath(sweep);
    g.restoreState();
  }

  // Special Highlight Strip solely for Values
  if (on && isValues) {
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(lamp.getX() + 1.0f, lamp.getY() + 0.5f, lamp.getRight() - 1.0f,
               lamp.getY() + 0.5f, 1.0f);
  }

  // Deep shadow line for recessed look
  g.setColour(juce::Colours::black.withAlpha(on ? 0.6f : 0.8f));
  g.drawLine(lamp.getX() + 1.0f, lamp.getBottom() - 0.5f,
             lamp.getRight() - 1.0f, lamp.getBottom() - 0.5f, 1.0f);

  // Symbols ONLY when OFF
  if (!on && isMainToggle) {
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    float cx = lamp.getCentreX();
    float cy = lamp.getCentreY();
    g.drawRect(cx - 0.75f, cy - 8.0f, 1.5f, 5.0f);
    g.drawEllipse(cx - 3.5f, cy + 3.0f, 7.0f, 7.0f, 1.5f);
  }

  if (button.getButtonText().isNotEmpty() &&
      (isMono || isValues || isTuner || buttonName == "punchButton" ||
       buttonName.containsIgnoreCase("auto"))) {
    float fontSize = (isMono || isTuner) ? 14.5f : (isValues ? 16.0f : 13.0f);
    g.setFont(juce::Font(fontSize, juce::Font::bold));

    // Vertical correction: move text down by 1.0px (Final polish for centering)
    auto textR = r.translated(0, 1.0f);

    if (on) {
      g.setColour(juce::Colours::black.withAlpha(0.9f));
      g.drawFittedText(button.getButtonText(), textR.toNearestInt(),
                       juce::Justification::centred, 1);
    } else {
      g.setColour(juce::Colours::white.withAlpha(
          (isMono || isTuner || isValues) ? 0.85f : 0.35f));
      g.drawFittedText(button.getButtonText(), textR.toNearestInt(),
                       juce::Justification::centred, 1);
    }
  }
}

void FunkyMooseLookAndFeel::drawButtonBackground(
    juce::Graphics &g, juce::Button &button,
    const juce::Colour &backgroundColour, bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown) {
  auto area = button.getLocalBounds().toFloat();
  const bool hover = shouldDrawButtonAsHighlighted;
  const bool down = shouldDrawButtonAsDown;
  g.setColour(
      juce::Colours::black.withAlpha(down ? 0.9f : (hover ? 0.8f : 0.7f)));
  g.fillRoundedRectangle(area, 4.0f);
  juce::Colour rimCol = juce::Colours::white.withAlpha(0.2f);
  if (button.getButtonText().containsIgnoreCase("Presets")) {
    rimCol = juce::Colour(0xfff0e040);
  }
  g.setColour(rimCol.withAlpha(hover ? 0.4f : 0.2f));
  g.drawRoundedRectangle(area.reduced(1.0f), 4.0f, 1.2f);
}

void FunkyMooseLookAndFeel::drawButtonText(juce::Graphics &g,
                                           juce::TextButton &button,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown) {
  auto area = button.getLocalBounds().toFloat();
  const bool hover = shouldDrawButtonAsHighlighted;
  const bool down = shouldDrawButtonAsDown;
  g.setFont(juce::Font(14.0f, juce::Font::bold));
  g.setColour(juce::Colours::black.withAlpha(0.8f));
  g.drawFittedText(button.getButtonText(), area.translated(1, 1).toNearestInt(),
                   juce::Justification::centred, 1);
  g.setColour(
      juce::Colours::white.withAlpha(down ? 1.0f : (hover ? 0.95f : 0.85f)));
  g.drawFittedText(button.getButtonText(), area.toNearestInt(),
                   juce::Justification::centred, 1);
}

void FunkyMooseLookAndFeel::drawBubble(juce::Graphics &g,
                                       juce::BubbleComponent &bubble,
                                       const juce::Point<float> &tip,
                                       const juce::Rectangle<float> &body) {
  g.setColour(juce::Colours::black.withAlpha(0.9f));
  g.fillRoundedRectangle(body, 6.0f);
  g.setColour(juce::Colours::white.withAlpha(0.18f));
  g.drawRoundedRectangle(body.reduced(0.5f), 6.0f, 1.2f);
}

// --- POPUP MENU STYLING ---
juce::Font FunkyMooseLookAndFeel::getPopupMenuFont() {
  return juce::Font(22.0f, juce::Font::bold); // Large & Bold for Presets
}

void FunkyMooseLookAndFeel::drawPopupMenuItem(
    juce::Graphics &g, const juce::Rectangle<int> &area, bool isSeparator,
    bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
    const juce::String &text, const juce::String &shortcutKeyText,
    const juce::Drawable *icon, const juce::Colour *textColour) {
  if (isSeparator) {
    auto r = area.reduced(5, 0);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawLine((float)r.getX(), (float)r.getCentreY(), (float)r.getRight(),
               (float)r.getCentreY(), 0.5f);
    return;
  }

  auto r = area.toFloat();

  if (isHighlighted && isActive) {
    // Warm Vintage Gold highlight
    g.setColour(juce::Colour(0xfff0e040).withAlpha(0.25f));
    g.fillRoundedRectangle(r.reduced(2.0f), 4.0f);
    g.setColour(juce::Colour(0xfff0e040).withAlpha(0.5f));
    g.drawRoundedRectangle(r.reduced(2.0f), 4.0f, 1.0f);
  }

  // Text Color logic
  g.setColour(isHighlighted ? juce::Colours::white
                            : juce::Colours::white.withAlpha(0.75f));
  if (!isActive)
    g.setColour(juce::Colours::white.withAlpha(0.2f));

  g.setFont(getPopupMenuFont());

  auto textRect = r.reduced(12.0f, 0);
  g.drawFittedText(text, textRect.toNearestInt(),
                   juce::Justification::centredLeft, 1);

  if (isTicked) {
    auto tickRect = r.removeFromLeft(24.0f).reduced(6.0f);
    g.setColour(juce::Colour(0xfff0e040));
    g.fillEllipse(tickRect.getCentreX() - 3, tickRect.getCentreY() - 3, 6, 6);
  }
}

void FunkyMooseLookAndFeel::drawPopupMenuSectionHeader(
    juce::Graphics &g, const juce::Rectangle<int> &area,
    const juce::String &sectionName) {
  g.setColour(juce::Colours::white.withAlpha(0.45f));
  g.setFont(juce::Font(16.0f, juce::Font::bold | juce::Font::italic));
  g.drawFittedText(sectionName, area.reduced(12, 0),
                   juce::Justification::centredLeft, 1);

  // Bottom underline for section header
  g.setColour(juce::Colours::white.withAlpha(0.1f));
  g.drawLine((float)area.getX(), (float)area.getBottom() - 1.0f,
             (float)area.getRight(), (float)area.getBottom() - 1.0f, 0.5f);
}
