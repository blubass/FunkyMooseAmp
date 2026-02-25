#include "FunkyMooseLookAndFeel.h"

FunkyMooseLookAndFeel::FunkyMooseLookAndFeel() {}

void FunkyMooseLookAndFeel::drawRotarySlider(
    juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider) {

  const juce::String id = slider.getComponentID();
  bool isMaster = (id == "MASTER");

  // Create a unique key for this knob type and size
  juce::String cacheKey =
      id + "_" + juce::String(width) + "x" + juce::String(height);
  auto &cache = knobCache[cacheKey];

  // --- Dynamic Color Logic ---
  juce::Colour baseColour = juce::Colour(0xff303030); // Default Dark
  if (id == "AMP")
    baseColour = juce::Colour::fromRGB(200, 50, 50);
  else if (id == "COMP")
    baseColour = juce::Colour::fromRGB(40, 90, 200);
  else if (id == "OCT")
    baseColour = juce::Colour::fromRGB(220, 130, 30);
  else if (id == "ENV")
    baseColour = juce::Colour::fromRGB(50, 160, 60);
  else if (id == "PH")
    baseColour = juce::Colour::fromRGB(160, 60, 170);
  else if (id == "CH")
    baseColour = juce::Colour::fromRGB(30, 170, 180);
  else if (id == "MASTER")
    baseColour = juce::Colour(0xff454545);
  else if (id.isEmpty())
    baseColour = juce::Colour::fromRGB(180, 180, 180);

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

    // --- DROP SHADOW ---
    {
      juce::DropShadow ds(
          juce::Colours::black.withAlpha(isMaster ? 0.94f : 0.78f), 4, {2, 3});
      juce::Path shadow;
      shadow.addEllipse(bounds);
      ds.drawForPath(ig, shadow);
    }

    // --- OUTER METAL RING ---
    auto outer = bounds;
    ig.setColour(juce::Colours::black.withAlpha(isMaster ? 0.45f : 0.35f));
    ig.fillEllipse(outer.translated(1.2f, 2.2f).reduced(6.0f));
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
    juce::ColourGradient ringGrad(
        juce::Colours::white.withAlpha(isMaster ? 0.65f : 0.47f), outer.getX(),
        outer.getY(), juce::Colours::black.withAlpha(isMaster ? 1.00f : 0.97f),
        outer.getRight(), outer.getBottom(), false);
    ig.setGradientFill(ringGrad);
    ig.fillPath(ring);
    ig.setColour(juce::Colours::black.withAlpha(0.95f));
    ig.drawEllipse(outer.reduced(3.0f), 1.0f);

    const int ticks = 36;
    auto ringR = (outer.getWidth() * 0.5f) - 2.0f;
    ig.setColour(juce::Colours::black.withAlpha(isMaster ? 0.08f : 0.18f));
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
    float bottomDarken = isMaster ? 0.86f : (id == "AMP" ? 0.79f : 0.75f);
    juce::ColourGradient capGrad(baseColour.brighter(0.4f), cx - radius * 0.4f,
                                 cy - radius * 0.4f,
                                 baseColour.darker(bottomDarken),
                                 cx + radius * 0.6f, cy + radius * 0.6f, true);
    ig.setGradientFill(capGrad);
    ig.fillEllipse(cap);
    ig.setColour(juce::Colours::black.withAlpha(0.65f));
    ig.drawEllipse(cap.reduced(1.0f), 1.2f);
    ig.setColour(juce::Colours::white.withAlpha(0.12f));
    ig.drawEllipse(cap.reduced(0.5f), 1.0f);
    if (id == "AMP") {
      ig.setColour(juce::Colours::white.withAlpha(0.18f));
      ig.drawEllipse(cap.reduced(1.4f), 1.0f);
    }
    ig.setColour(juce::Colours::black.withAlpha(id == "AMP" ? 0.65f : 0.55f));
    ig.drawEllipse(cap.reduced(2.2f), 1.0f);
    if (isMaster) {
      ig.setColour(juce::Colours::black.withAlpha(0.25f));
      ig.drawEllipse(cap.reduced(3.5f), 1.2f);
    }

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

  // Specular Highlight (SSL shine)
  {
    float hiReduce = isMaster ? 0.885f : (id == "AMP" ? 0.86f : 0.84f);
    auto hi = cap.reduced(cap.getWidth() * hiReduce);
    float specX =
        cx - cap.getWidth() * 0.30f + std::cos(angle) * radius * 0.12f;
    float specY =
        cy - cap.getHeight() * 0.30f + std::sin(angle) * radius * 0.12f;
    hi.setPosition(specX, specY);
    float highlightAlpha = isMaster ? 0.42f : (id == "AMP" ? 0.34f : 0.30f);
    juce::ColourGradient shine(juce::Colours::white.withAlpha(highlightAlpha),
                               hi.getCentreX(), hi.getCentreY(),
                               juce::Colours::transparentWhite, hi.getRight(),
                               hi.getBottom(), true);
    g.setGradientFill(shine);
    g.fillEllipse(hi);
    float specAlpha = isMaster ? 0.35f : 0.25f;
    g.setColour(juce::Colours::white.withAlpha(specAlpha));
    g.drawEllipse(hi.reduced(hi.getWidth() * 0.3f), 1.2f);
    float pingAlpha = isMaster ? 0.70f : 0.55f;
    float pingX = cx - radius * 0.45f + std::cos(angle - 0.3f) * radius * 0.08f;
    float pingY = cy - radius * 0.45f + std::sin(angle - 0.3f) * radius * 0.08f;
    g.setColour(juce::Colours::white.withAlpha(pingAlpha));
    g.fillEllipse(pingX, pingY, 2.0f, 2.0f);
  }

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

  // Indicator
  {
    float pointerLength = radius * 0.75f;
    float pointerWidth = juce::jmax(1.8f, radius * 0.088f);
    juce::Point<float> p1(cx, cy);
    juce::Point<float> p2(cx + std::cos(angle) * pointerLength,
                          cy + std::sin(angle) * pointerLength);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawLine(p1.x + 1.5f, p1.y + 1.5f, p2.x + 1.5f, p2.y + 1.5f,
               pointerWidth);
    g.setColour(juce::Colours::white.withAlpha(isMaster ? 1.0f : 0.9f));
    g.drawLine(p1.x, p1.y, p2.x, p2.y, pointerWidth);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawLine(p1.x, p1.y, p2.x, p2.y, pointerWidth * 0.4f);
  }

  // Moose Emboss
  if (slider.getName() == "GAIN_AMP" || slider.getName() == "MASTER_OUT") {
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
  }
}

void FunkyMooseLookAndFeel::drawToggleButton(juce::Graphics &g,
                                             juce::ToggleButton &button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown) {
  auto r = button.getLocalBounds().toFloat().reduced(2.0f);

  const bool on = button.getToggleState();
  const bool hover =
      button.isMouseOverOrDragging() || shouldDrawButtonAsHighlighted;
  const bool down = button.isMouseButtonDown() || shouldDrawButtonAsDown;

  g.setColour(juce::Colours::black.withAlpha(0.45f));
  g.fillRoundedRectangle(r, 3.0f);
  g.setColour(juce::Colours::black.withAlpha(0.65f));
  g.drawRoundedRectangle(r, 3.0f, 1.2f);

  auto lamp = r.reduced(4.0f);
  // Re-use dynamic color logic just in case, but usually fixed in header
  // Using accentColor from class member
  // Special cyan color for auto-gain toggles
  auto buttonName = button.getName();
  bool isAutoGain =
      (buttonName == "ampAutoGain" || buttonName == "compAutoMakeup" ||
       buttonName == "autoGain");
  auto lampCol = on ? (isAutoGain ? juce::Colour(0xff44ccff) : accentColor)
                    : juce::Colours::white.withAlpha(0.10f);

  float glowA = on ? 0.65f : (hover ? 0.18f : 0.10f);
  if (down)
    glowA *= 1.2f;

  // Subtle pulsing afterglow for active buttons (micro-animation)
  if (on) {
    float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
    float pulse = 0.85f + 0.15f * std::sin(time * 1.5f); // Slow, subtle pulse
    glowA *= pulse;
  }

  g.setColour(lampCol.withAlpha(glowA * 0.35f));
  g.fillRoundedRectangle(lamp.expanded(3.0f), 4.0f);

  g.setColour(lampCol.withAlpha(glowA));
  g.fillRoundedRectangle(lamp, 3.0f);

  juce::ColourGradient grad(juce::Colours::white.withAlpha(on ? 0.12f : 0.06f),
                            lamp.getCentreX(), lamp.getY(),
                            juce::Colours::transparentWhite, lamp.getCentreX(),
                            lamp.getBottom(), false);
  g.setGradientFill(grad);
  g.fillRoundedRectangle(lamp, 3.0f);

  // Draw Text (if any)
  if (button.getButtonText().isNotEmpty()) {
    g.setColour(juce::Colours::white.withAlpha(on ? 0.95f : 0.5f));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), r.toNearestInt(),
                     juce::Justification::centred, 1);
  }
}

void FunkyMooseLookAndFeel::drawBubble(juce::Graphics &g,
                                       juce::BubbleComponent &bubble,
                                       const juce::Point<float> &tip,
                                       const juce::Rectangle<float> &body) {
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillRoundedRectangle(body.translated(2.0f, 2.0f), 6.0f);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRoundedRectangle(body, 6.0f);
  g.setColour(accentColor);
  g.drawRoundedRectangle(body, 6.0f, 1.2f);
}
