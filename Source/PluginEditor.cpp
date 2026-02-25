#include "PluginEditor.h"
#include "BinaryData.h"

static void initKnob(juce::Slider &s) {
  s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  s.setRange(0.0, 1.0, 0.0);
  s.setSkewFactor(1.0);
}

FunkyMooseAudioProcessorEditor::FunkyMooseAudioProcessorEditor(
    FunkyMooseAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p), inVu(*this), outVu(*this) {
  setLookAndFeel(&lookAndFeel);

  // Helper: load an image by trying multiple BinaryData resource names.
  // (The generated symbol name depends on the original filename.)
  const auto loadImageByName = [](std::initializer_list<const char *> names) {
    for (auto *n : names) {
      int dataSize = 0;
      if (auto *data = BinaryData::getNamedResource(n, dataSize))
        return juce::ImageCache::getFromMemory(data, dataSize);
    }

    // Fallback: scan BinaryData for anything that looks like an elch image
    for (int i = 0; BinaryData::namedResourceList[i] != nullptr; ++i) {
      const juce::String nm(BinaryData::namedResourceList[i]);

      if (nm.containsIgnoreCase("elch") && nm.containsIgnoreCase("png")) {
        int dataSize = 0;
        if (auto *data = BinaryData::getNamedResource(
                BinaryData::namedResourceList[i], dataSize))
          return juce::ImageCache::getFromMemory(data, dataSize);
      }
    }

    return juce::Image{};
  };

  // Load UI assets from BinaryData (keeps it crisp + self-contained)
  lookAndFeel.setKnobBig(juce::ImageCache::getFromMemory(
      BinaryData::knob_big_png, BinaryData::knob_big_pngSize));
  lookAndFeel.setKnobSmall(juce::ImageCache::getFromMemory(
      BinaryData::knob_small_png, BinaryData::knob_small_pngSize));
  lookAndFeel.setKnobSpec(juce::ImageCache::getFromMemory(
      BinaryData::knob_spec_png, BinaryData::knob_spec_pngSize));
  lookAndFeel.setKnobScratches(juce::ImageCache::getFromMemory(
      BinaryData::knob_scratches_png, BinaryData::knob_scratches_pngSize));
  lookAndFeel.setMetalOverlay(juce::ImageCache::getFromMemory(
      BinaryData::metal_tile_png, BinaryData::metal_tile_pngSize));

  // Elch image (try common names; keeps the project compiling even if the asset
  // filename changes)
  if (auto img =
          loadImageByName({"elch_new.png", "elch.png", "elch_new", "elch"});
      img.isValid())
    elch.setElchImage(img);

  addAndMakeVisible(content);
  content.setOpaque(true);
  setOpaque(true);

  // We are using a manually managed Framebuffer Cache (cachedContentBackground)
  // for the static background to achieve maximum performance.
  // Dynamic elements (Meters, Glows) are drawn on top.

  for (auto *k : {&gainKnob,     &bassKnob,    &midKnob,        &trebleKnob,
                  &volumeKnob,   &compInKnob,  &compThreshKnob, &compMakeKnob,
                  &compAtkKnob,  &compRelKnob, &oct1Knob,       &oct2Knob,
                  &octMixKnob,   &envAtkKnob,  &envDecKnob,     &envRangeKnob,
                  &phRateKnob,   &phColKnob,   &phMixKnob,      &chRateKnob,
                  &chDepthKnob,  &chMixKnob,   &outKnob,        &mixKnob,
                  &monoMakerKnob}) {
    content.addAndMakeVisible(*k);

    // Init Logic (Inline) + POPUP ENABLED
    k->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k->setRange(0.0, 1.0, 0.0);
    k->setSkewFactor(1.0);

    // Enable popup bubble on Drag (true) and Hover (true)
    // Parent is *this* (the editor), so it floats over everything
    k->setPopupDisplayEnabled(true, true, this);
    k->setNumDecimalPlacesToDisplay(2);
  }

  // Set Knob Groups for Color Coding (Eden Style)
  auto setGroup = [&](std::initializer_list<juce::Slider *> sliders,
                      juce::String id) {
    for (auto *s : sliders)
      s->setComponentID(id);
  };

  setGroup({&gainKnob, &bassKnob, &midKnob, &trebleKnob, &volumeKnob}, "AMP");
  setGroup(
      {&compInKnob, &compThreshKnob, &compMakeKnob, &compAtkKnob, &compRelKnob},
      "COMP");
  setGroup({&oct1Knob, &oct2Knob, &octMixKnob}, "OCT");
  setGroup({&envAtkKnob, &envDecKnob, &envRangeKnob}, "ENV");
  setGroup({&phRateKnob, &phColKnob, &phMixKnob}, "PH");
  setGroup({&chRateKnob, &chDepthKnob, &chMixKnob}, "CH");
  setGroup({&outKnob, &mixKnob, &monoMakerKnob}, "MASTER");

  // Specific Branding for VIP knobs
  gainKnob.setName("GAIN_AMP");
  outKnob.setName("MASTER_OUT");

  // Cab Button Logic
  cabButton.setClickingTogglesState(false);
  cabButton.onClick = [this] {
    auto *p = processor.apvts.getParameter("cabType");
    auto *raw = processor.apvts.getRawParameterValue("cabType");
    if (p && raw) {
      // Robust cycling using Raw Index (0, 1, 2)
      int currentIdx = (int)std::round(raw->load());
      int nextIdx = (currentIdx + 1) % 3;
      // Convert to Normalized (0.0, 0.5, 1.0) for Choice(3)
      float nextNorm = (float)nextIdx / 2.0f;

      p->beginChangeGesture();
      p->setValueNotifyingHost(nextNorm);
      p->endChangeGesture();

      // Immediate visual update
      juce::String txt =
          (nextIdx == 0) ? "OFF" : ((nextIdx == 1) ? "4x10" : "1x15");
      cabButton.setButtonText(txt);
    }
  };
  content.addAndMakeVisible(cabButton);

  slapToggle.setButtonText("");
  content.addAndMakeVisible(slapToggle);

  tubeToggle.setButtonText("");
  content.addAndMakeVisible(tubeToggle);

  ampOnToggle.setButtonText("");
  content.addAndMakeVisible(ampOnToggle);

  lowCutToggle.setButtonText("");
  content.addAndMakeVisible(lowCutToggle);

  ampAutoGainToggle.setButtonText("");
  ampAutoGainToggle.setName("ampAutoGain");
  content.addAndMakeVisible(ampAutoGainToggle);

  compAutoMakeupToggle.setButtonText("");
  compAutoMakeupToggle.setName("compAutoMakeup");
  content.addAndMakeVisible(compAutoMakeupToggle);

  ratioBox.addItem("4:1", 1);
  ratioBox.addItem("8:1", 2);
  ratioBox.addItem("12:1", 3);
  ratioBox.addItem("20:1", 4);
  ratioBox.setSelectedId(1);
  content.addAndMakeVisible(ratioBox);

  // Module toggles (tiny lamps)
  for (auto *t : {&compOn, &octOn, &envOn, &phaserOn, &chorusOn, &masterOn,
                  &octModernToggle, &fxParallelToggle}) {
    t->setButtonText("");
    t->setToggleState(t == &octModernToggle || t == &fxParallelToggle ? false
                                                                      : true,
                      juce::dontSendNotification);
    content.addAndMakeVisible(*t);
  }

  // Punch button (separate on purpose: it's a real button, not a tiny lamp)
  punchButton.setClickingTogglesState(true);
  content.addAndMakeVisible(punchButton);

  // NEW: Tooltip Toggle
  content.addAndMakeVisible(toggleTooltips);
  toggleTooltips.setButtonText("Values");
  toggleTooltips.setToggleState(true, juce::dontSendNotification);
  toggleTooltips.onClick = [this] {
    const bool show = toggleTooltips.getToggleState();
    for (auto *k :
         {&gainKnob,    &bassKnob,    &midKnob,        &trebleKnob,
          &volumeKnob,  &compInKnob,  &compThreshKnob, &compMakeKnob,
          &compAtkKnob, &compRelKnob, &oct1Knob,       &oct2Knob,
          &octMixKnob,  &envAtkKnob,  &envDecKnob,     &envRangeKnob,
          &phRateKnob,  &phColKnob,   &phMixKnob,      &chRateKnob,
          &chDepthKnob, &chMixKnob,   &outKnob,        &monoMakerKnob}) {
      k->setPopupDisplayEnabled(show, show, this);
    }
  };

  autoGainToggle.setButtonText("");
  autoGainToggle.setName("autoGain");
  content.addAndMakeVisible(autoGainToggle);

  autoGateToggle.setButtonText("");
  content.addAndMakeVisible(autoGateToggle);

  monoMakerToggle.setButtonText("");
  content.addAndMakeVisible(monoMakerToggle);

  tunerToggle.setButtonText("TUNER");
  tunerToggle.setClickingTogglesState(true);
  tunerToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
  content.addAndMakeVisible(tunerToggle);

  tunerOverlay = std::make_unique<TunerComponent>(processor.getTunerFifo(),
                                                  processor.getTunerFlag());
  tunerOverlay->prepare(processor.getSampleRate());
  content.addChildComponent(*tunerOverlay);

  tunerToggle.onClick = [this] {
    const bool on = tunerToggle.getToggleState();
    tunerOverlay->setVisible(on);
    inVu.setVisible(!on);
  };

  content.addAndMakeVisible(statsHUD);
  statsHUD.setInterceptsMouseClicks(false, false);

  // PRESETS Header (Must be Added to Content!)
  content.addAndMakeVisible(presetSelector);
  presetSelector.setButtonText("Presets...");

  content.addAndMakeVisible(savePresetButton);
  content.addAndMakeVisible(openFolderButton);

  content.addAndMakeVisible(inVu);
  content.addAndMakeVisible(outVu);
  content.addAndMakeVisible(elch);
  elch.setInterceptsMouseClicks(false, false);
  elch.setAlwaysOnTop(true);
  elch.toFront(false);

  content.addAndMakeVisible(overlayComp);
  overlayComp.setInterceptsMouseClicks(false, false);
  overlayComp.setImagePlacement(juce::RectanglePlacement::stretchToFit);

  // Preset UI
  presetSelector.onClick = [this] {
    // Build menu
    juce::PopupMenu m;

    // Factory Section
    m.addSectionHeader("Factory Bank");
    for (const auto &p : processor.getPresetList()) {
      if (p.startsWith("F:")) {
        juce::String name = p.substring(2);
        m.addItem(name, [this, name] {
          processor.loadPreset(name);
          presetSelector.setButtonText(name);
        });
      }
    }

    // User Section
    m.addSeparator();
    m.addSectionHeader("User Bank");

    bool hasUser = false;
    for (const auto &p : processor.getPresetList()) {
      if (p.startsWith("U:")) {
        hasUser = true;
        juce::String name = p.substring(2);
        m.addItem(name, [this, name] {
          processor.loadPreset(name);
          presetSelector.setButtonText(name);
        });
      }
    }

    if (!hasUser)
      m.addItem("(No User Presets)", false, false, nullptr);

    m.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(presetSelector));
  };

  savePresetButton.onClick = [this] {
    auto *aw = new juce::AlertWindow(
        "Save Preset",
        "Enter a name for your preset:", juce::AlertWindow::NoIcon);
    aw->addTextEditor("name", "New Preset", "");
    aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(
        true, juce::ModalCallbackFunction::create([this, aw](int result) {
          if (result == 1) {
            juce::String name = aw->getTextEditorContents("name");
            if (name.isNotEmpty()) {
              processor.savePreset(name);
              presetSelector.setButtonText(name);
            }
          }
          delete aw;
        }));
  };

  openFolderButton.onClick = [this] {
    processor.getPresetsFolder().revealToUser();
  };

  // Enforce 16:9 Aspect Ratio (2048 x 1152)
  double ratio = (double)designW / (double)designH;
  setResizable(true, true);
  getConstrainer()->setFixedAspectRatio(ratio);

  // Min: 1024x576 (50%), Max: 3072x1728 (150%)
  setResizeLimits(1024, (int)(1024.0 / ratio), 3072, (int)(3072.0 / ratio));

  // Start Size: ~1280x720 (Standard HD) or slightly larger
  setSize(1400, (int)(1400.0 / ratio));

  // CONNECTIONS
  using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
  using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
  using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

  auto &ts = processor.apvts;

  // Hardcoded "Bass Strategy" (Vintage) Look
  const int skinIndex = 3;

  currentPalette = Skins::getPalette(skinIndex);

  // Set L&F colors immediately
  lookAndFeel.setColors(currentPalette.accent, currentPalette.knob,
                        currentPalette.knobIndicator);
  elch.setColors(currentPalette.elchEye, currentPalette.elchGlow);
  elch.setBackgroundColor(currentPalette.accent.darker().withAlpha(0.2f));

  // Load Assets
  auto elchImg = juce::ImageCache::getFromMemory(
      BinaryData::elch_vintage_png, BinaryData::elch_vintage_pngSize);
  elch.setElchImage(elchImg);

  auto rawFrame = juce::ImageCache::getFromMemory(
      BinaryData::FrameOverlay_png, BinaryData::FrameOverlay_pngSize);

  if (rawFrame.isValid()) {
    juce::Image frameImg = rawFrame.createCopy();
    juce::Image::BitmapData data(frameImg, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < data.height; ++y) {
      for (int x = 0; x < data.width; ++x) {
        auto c = data.getPixelColour(x, y);
        if (c.getBrightness() > 0.9f && c.getSaturation() < 0.1f) {
          data.setPixelColour(x, y, juce::Colours::transparentBlack);
        }
      }
    }
    overlayComp.setImage(frameImg);
  } else {
    overlayComp.setImage(juce::Image());
  }

  // Amp
  slapAtt = std::make_unique<BA>(ts, "slap", slapToggle);
  tubeAtt = std::make_unique<BA>(ts, "tubeOn", tubeToggle);
  lowCutAtt = std::make_unique<BA>(ts, "lowCutOn", lowCutToggle);
  ampOnAtt = std::make_unique<BA>(ts, "ampOn", ampOnToggle);
  gainAtt = std::make_unique<SA>(ts, "ampGain", gainKnob);
  bassAtt = std::make_unique<SA>(ts, "ampBass", bassKnob);
  midAtt = std::make_unique<SA>(ts, "ampMid", midKnob);
  trebleAtt = std::make_unique<SA>(ts, "ampTreble", trebleKnob);
  volAtt = std::make_unique<SA>(ts, "ampVolume", volumeKnob);
  ampAutoGainAtt = std::make_unique<BA>(ts, "ampAutoGain", ampAutoGainToggle);

  // Comp
  compOnAtt = std::make_unique<BA>(ts, "compOn", compOn);
  punchAtt = std::make_unique<BA>(ts, "punch", punchButton);
  compInAtt = std::make_unique<SA>(ts, "compInput", compInKnob);
  compThrAtt = std::make_unique<SA>(ts, "compThresh", compThreshKnob);
  compMkAtt = std::make_unique<SA>(ts, "compMakeup", compMakeKnob);
  compAtkAtt = std::make_unique<SA>(ts, "compAttack", compAtkKnob);
  compRelAtt = std::make_unique<SA>(ts, "compRelease", compRelKnob);
  compAutoMakeupAtt =
      std::make_unique<BA>(ts, "compAutoMakeup", compAutoMakeupToggle);
  tunerAttachment = std::make_unique<BA>(ts, "tunerOn", tunerToggle);
  compRatAtt = std::make_unique<CA>(ts, "compRatio", ratioBox);

  // FX
  octOnAtt = std::make_unique<BA>(ts, "octOn", octOn);
  oct1Att = std::make_unique<SA>(ts, "oct1", oct1Knob);
  oct2Att = std::make_unique<SA>(ts, "oct2", oct2Knob);
  octMixAtt = std::make_unique<SA>(ts, "octMix", octMixKnob);
  octModernAtt = std::make_unique<BA>(ts, "octModern", octModernToggle);

  envOnAtt = std::make_unique<BA>(ts, "envOn", envOn);
  envAtkAtt = std::make_unique<SA>(ts, "envAttack", envAtkKnob);
  envDecAtt = std::make_unique<SA>(ts, "envDecay", envDecKnob);
  envRngAtt = std::make_unique<SA>(ts, "envRange", envRangeKnob);

  masterOnAtt = std::make_unique<BA>(ts, "masterOn", masterOn);
  autoGateAtt = std::make_unique<BA>(ts, "autoGate", autoGateToggle);

  phOnAtt = std::make_unique<BA>(ts, "phaserOn", phaserOn);
  phRateAtt = std::make_unique<SA>(ts, "phRate", phRateKnob);
  phColAtt = std::make_unique<SA>(ts, "phColour", phColKnob);
  phMixAtt = std::make_unique<SA>(ts, "phMix", phMixKnob);

  chOnAtt = std::make_unique<BA>(ts, "chorusOn", chorusOn);
  chRateAtt = std::make_unique<SA>(ts, "chRate", chRateKnob);
  chDepthAtt = std::make_unique<SA>(ts, "chDepth", chDepthKnob);
  chMixAtt = std::make_unique<SA>(ts, "chMix", chMixKnob);

  outAtt = std::make_unique<SA>(ts, "masterOut", outKnob);
  mixAtt = std::make_unique<SA>(ts, "masterMix", mixKnob);
  monoMakerAtt = std::make_unique<SA>(ts, "monoMaker", monoMakerKnob);
  autoGainAtt = std::make_unique<BA>(ts, "autoGain", autoGainToggle);
  monoMakerOnAtt = std::make_unique<BA>(ts, "monoMakerOn", monoMakerToggle);
  fxParallelAtt = std::make_unique<BA>(ts, "fxParallel", fxParallelToggle);

  // Force 3 decimal places for popups (overriding attachment defaults)
  for (auto *k : {&gainKnob,     &bassKnob,    &midKnob,        &trebleKnob,
                  &volumeKnob,   &compInKnob,  &compThreshKnob, &compMakeKnob,
                  &compAtkKnob,  &compRelKnob, &oct1Knob,       &oct2Knob,
                  &octMixKnob,   &envAtkKnob,  &envDecKnob,     &envRangeKnob,
                  &phRateKnob,   &phColKnob,   &phMixKnob,      &chRateKnob,
                  &chDepthKnob,  &chMixKnob,   &outKnob,        &mixKnob,
                  &monoMakerKnob}) {
    k->textFromValueFunction = [](double value) {
      return juce::String(value, 2);
    };
  }

  updateGlowCaches();
  startTimerHz(30);
}

FunkyMooseAudioProcessorEditor::~FunkyMooseAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

struct FunkyMooseAudioProcessorEditor::LayoutRects {
  juce::Rectangle<float> plate;
  juce::Rectangle<float> topBar;

  juce::Rectangle<float> meter;
  juce::Rectangle<float> amp;
  juce::Rectangle<float> comp;
  juce::Rectangle<float> fx;
  juce::Rectangle<float> master;
  juce::Rectangle<float> outVuArea;
  juce::Rectangle<float> elchArea;

  std::array<juce::Rectangle<float>, 4> fxSlots;
  std::array<juce::String, 4> fxNames{"OCTAVER", "ENVELOPE", "PHASER",
                                      "CHORUS"};
};

void FunkyMooseAudioProcessorEditor::ensureCachedTextures(int skinIndex) {
  // Plate cache: contains pitting and scratch overlays drawn into a
  // transparent image the size of the design canvas.
  if (cachedPlateTexture.isNull() || cachedPlateTexture.getWidth() != designW ||
      cachedPlateTexture.getHeight() != designH) {
    cachedPlateTexture = juce::Image(juce::Image::ARGB, designW, designH, true);
    juce::Graphics ig(cachedPlateTexture);
    ig.fillAll(juce::Colours::transparentBlack);

    auto L = getLayout();
    auto plate = L.plate;

    juce::Random rng(42);

    // Layer 1: Heavy pitting/corrosion (Dark)
    ig.setColour(juce::Colours::black.withAlpha(0.25f));
    for (int i = 0; i < 2000; ++i) {
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float w = 5.0f + rng.nextFloat() * 40.0f;
      float h = 1.0f + rng.nextFloat() * 2.0f;
      ig.fillRect(x, y, w, h);
    }

    // Layer 2: Brass/Gold Scratches (Highlights)
    ig.setColour(juce::Colour::fromRGB(255, 200, 100).withAlpha(0.08f));
    for (int i = 0; i < 1000; ++i) {
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float w = 5.0f + rng.nextFloat() * 30.0f;
      float h = 0.8f;
      ig.fillRect(x, y, w, h);
    }
  }

  // Skin overlays (procedural) - only generate for the special skins
  if (skinIndex == 4 || skinIndex == 5) {
    if (cachedSkinOverlay.isNull() || cachedSkinOverlay.getWidth() != designW ||
        cachedSkinOverlay.getHeight() != designH ||
        cachedSkinIndex != skinIndex) {
      cachedSkinOverlay =
          juce::Image(juce::Image::ARGB, designW, designH, true);
      juce::Graphics ig(cachedSkinOverlay);
      ig.fillAll(juce::Colours::transparentBlack);

      if (skinIndex == 4) {
        juce::Random r(1234);
        ig.setColour(juce::Colours::black.withAlpha(0.08f));
        for (int i = 0; i < 400; ++i) {
          float rx = r.nextFloat() * (float)getWidth();
          float ry = r.nextFloat() * (float)getHeight();
          ig.fillEllipse(rx, ry, r.nextFloat() * 2.5f + 0.5f,
                         r.nextFloat() * 2.5f + 0.5f);
        }
      } else if (skinIndex == 5) {
        juce::Random r(666);

        // Fine Background Spray (Mist)
        ig.setColour(juce::Colour(0x55440000));
        for (int i = 0; i < 600; ++i) {
          float rx = r.nextFloat() * (float)getWidth();
          float ry = r.nextFloat() * (float)getHeight();
          ig.fillEllipse(rx, ry, r.nextFloat() * 1.5f + 0.2f,
                         r.nextFloat() * 1.5f + 0.2f);
        }

        // Splatter Clusters
        for (int i = 0; i < 45; ++i) {
          float impactX = r.nextFloat() * (float)getWidth();
          float impactY = r.nextFloat() * (float)getHeight();

          const bool isFresh = r.nextFloat() > 0.65f;
          const auto bloodCol =
              isFresh ? juce::Colour(0xccaa0000) : juce::Colour(0xbb660000);
          ig.setColour(bloodCol.withAlpha(0.6f + r.nextFloat() * 0.3f));

          float mainSize = r.nextFloat() * 30.0f + 8.0f;
          ig.fillEllipse(impactX - mainSize / 2, impactY - mainSize / 2,
                         mainSize, mainSize * (0.85f + r.nextFloat() * 0.3f));

          int satellites = r.nextInt(6) + 2;
          for (int j = 0; j < satellites; ++j) {
            float angle = r.nextFloat() * juce::MathConstants<float>::twoPi;
            float dist = mainSize * (0.6f + r.nextFloat() * 0.8f);
            float satSize = r.nextFloat() * (mainSize * 0.4f) + 1.0f;
            ig.fillEllipse(impactX + std::cos(angle) * dist - satSize / 2,
                           impactY + std::sin(angle) * dist - satSize / 2,
                           satSize, satSize);
          }

          if (r.nextFloat() > 0.5f) {
            float dripW = mainSize * (0.1f + r.nextFloat() * 0.15f);
            float dripL = mainSize * (0.5f + r.nextFloat() * 3.0f);
            ig.fillRect(impactX - dripW / 2, impactY + mainSize * 0.2f, dripW,
                        dripL);
            ig.fillEllipse(impactX - dripW / 2,
                           impactY + mainSize * 0.2f + dripL - dripW / 2, dripW,
                           dripW);
          }
        }
      }
      cachedSkinIndex = skinIndex;
    }
  } else {
    // Clear overlay when not needed
    cachedSkinOverlay = juce::Image();
  }
  cachedSkinIndex = skinIndex;
}

// This method now only renders static content to the CACHE
void FunkyMooseAudioProcessorEditor::updateStaticBackground() {
  if (designW <= 0 || designH <= 0)
    return;

  cachedContentBackground =
      juce::Image(juce::Image::ARGB, designW, designH, true);
  juce::Graphics g(cachedContentBackground);

  // ===== Brushed Metal Base =====
  auto area = juce::Rectangle<float>(0, 0, (float)designW, (float)designH);

  // dunkle Grundfarbe
  g.fillAll(juce::Colour::fromRGB(34, 32, 30));

  // leichter vertikaler Brush-Gradient
  juce::ColourGradient metalGrad(juce::Colour::fromRGB(60, 58, 55), 0,
                                 area.getY(), juce::Colour::fromRGB(28, 26, 24),
                                 0, area.getBottom(), false);

  g.setGradientFill(metalGrad);
  g.fillRect(area);

  // leichte Vignette für Tiefe
  juce::ColourGradient vignette(
      juce::Colours::transparentBlack, area.getCentreX(), area.getCentreY(),
      juce::Colours::black.withAlpha(0.6f), area.getX(), area.getY(), true);

  g.setGradientFill(vignette);
  g.fillRect(area);

  // Subtle noise
  g.saveState();
  g.setColour(juce::Colours::white.withAlpha(0.03f));
  for (int i = 0; i < designH; i += 3)
    g.fillRect(0, i, designW, 1);
  g.restoreState();

  const auto L = getLayout();

  // Main plate - Heavy Brushed Industrial Metal
  {
    g.saveState();
    auto plate = L.plate;

    // 1. Base Metal: VINTAGE INDUSTRIAL BRONZE (Dark & Warm)
    // Deep, heavy dark metal look
    juce::Colour c1 = juce::Colour::fromRGB(55, 45, 40); // Dark Bronze/Brown
    juce::Colour c2 = juce::Colour::fromRGB(25, 20, 18); // Deep Oxide

    juce::ColourGradient cg(c1, plate.getX(), plate.getY(), c2,
                            plate.getRight(), plate.getBottom(), false);

    // Warm rich highlights (Copper/Gold tint)
    cg.addColour(0.3f, juce::Colour::fromRGB(80, 65, 55));
    cg.addColour(0.7f, juce::Colour::fromRGB(45, 35, 30));

    g.setGradientFill(cg);
    g.fillRoundedRectangle(plate, currentPalette.cornerRadius);

    // 2. Heavy Brushed Texture (Horizontal Grain)
    // Render expensive noise/scratch layers from a cached image when
    // available. This reduces per-frame CPU during continuous repaint.
    // Ensure cache exists for current skin
    {
      // Determine skin index safely
      int skinIdx = 0;
      if (auto *raw = processor.apvts.getRawParameterValue("skin"))
        skinIdx = (int)std::round(raw->load());

      ensureCachedTextures(skinIdx);

      if (!cachedPlateTexture.isNull()) {
        // Draw cached overlay into the plate area
        g.drawImage(cachedPlateTexture, plate.getX(), plate.getY(),
                    plate.getWidth(), plate.getHeight(), 0, 0,
                    cachedPlateTexture.getWidth(),
                    cachedPlateTexture.getHeight(), false);
      }
    }

    // 3. 3D Beveled Edge - Crisp Industrial
    float cr = currentPalette.cornerRadius;

    // Top Highlight (Sharp Chrome Edge)
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawRoundedRectangle(plate.expanded(1.0f), cr, 2.0f);

    // Bottom Shadow (Drop Shadow)
    for (int i = 0; i < 8; ++i) {
      float d = 2.0f + i * 1.5f;
      float a = 0.5f - i * 0.05f;
      g.setColour(juce::Colours::black.withAlpha(a));
      g.drawRoundedRectangle(plate.translated(d * 0.5f, d * 0.5f), cr, 2.0f);
    }

    // 4. Corner Screws (Keep as is, they look good on lighter metal too)
    auto drawScrew = [&](juce::Point<float> p) {
      float s = 30.0f; // Increased from 28
      float r = s / 2.0f;
      juce::Rectangle<float> sr(p.x - r, p.y - r, s, s);

      // Deep Hole Shadow (Darker & Larger)
      g.setColour(juce::Colours::black.withAlpha(0.95f));
      g.fillEllipse(sr.translated(1.0f, 1.0f).expanded(1.5f));

      // Screw Body (Gold/Brass) - Slightly darker for subtlety
      juce::ColourGradient sg(juce::Colour(0xffd8b048), sr.getX(), sr.getY(),
                              juce::Colour(0xff423513), sr.getRight(),
                              sr.getBottom(), false);
      sg.addColour(0.3f, juce::Colour(0xffe8e0c8)); // Specular (also darker)
      g.setGradientFill(sg);
      g.fillEllipse(sr);

      // --- Stronger Inner Shadow for Depth ---
      g.setColour(juce::Colours::black.withAlpha(0.65f));
      g.drawEllipse(sr.reduced(1.0f), 2.0f);

      // Inner Ring Texture
      g.setColour(juce::Colours::black.withAlpha(0.4f));
      g.drawEllipse(sr.reduced(3.5f), 1.5f);

      // Cross Slot - Deeply cut
      auto slot1 = sr.reduced(6.0f, 11.0f);
      auto slot2 = sr.reduced(11.0f, 6.0f);

      g.setColour(juce::Colours::black.withAlpha(0.95f));
      g.fillRoundedRectangle(slot1, 2.0f);
      g.fillRoundedRectangle(slot2, 2.0f);

      // Edge Highlights on slot
      g.setColour(juce::Colours::white.withAlpha(0.8f));
      g.drawRoundedRectangle(slot1.translated(0.8f, 0.8f), 2.0f, 1.0f);
    };

    // Increased inset margin for screws to prevent clipping
    float m = 24.0f;
    drawScrew(plate.getTopLeft().translated(m, m));
    drawScrew(plate.getTopRight().translated(-m, m));
    drawScrew(plate.getBottomLeft().translated(m, -m));
    drawScrew(plate.getBottomRight().translated(-m, -m));

    g.restoreState();
  }

  // Frame style
  const auto frameCol = currentPalette.accent.withAlpha(0.5f);
  const auto txt = currentPalette.labelText;
  const auto sub = currentPalette.labelText.withAlpha(0.6f);

  auto drawFrame = [&](juce::Rectangle<float> r, float radius = -1.0f,
                       float stroke = -1.0f,
                       juce::Colour colOverride =
                           juce::Colours::transparentBlack,
                       bool fill = false, float depthBias = 0.0f,
                       float darkenFactor = 1.0f,
                       juce::Colour colorTint = juce::Colours::transparentBlack,
                       int surfaceType =
                           0) { // 0=brushed, 1=anodized, 2=polished
    const float curRadius = (radius < 0) ? currentPalette.cornerRadius : radius;
    const float curStroke = (stroke < 0) ? currentPalette.frameWidth : stroke;
    const auto baseCol =
        colOverride.isTransparent() ? currentPalette.accent : colOverride;

    // Recessed Panel Background -> NOW UNIFIED FLAT METAL
    if (fill) {
      g.saveState();
      g.reduceClipRegion(r.expanded(2.0f).toNearestInt());

      // 1. RECESSED VINTAGE PANEL (Aged Copper/Bronze) - Now more matte
      // Apply darkening factor for visual hierarchy
      juce::Colour c1 = juce::Colour::fromRGB(
          (int)(78 * darkenFactor), (int)(65 * darkenFactor),
          (int)(52 * darkenFactor)); // Darkened top
      juce::Colour c2 = juce::Colour::fromRGB(
          (int)(36 * darkenFactor), (int)(28 * darkenFactor),
          (int)(23 * darkenFactor)); // Darkened bottom

      // Gradient: Smooth vertical/radial feel (top lighter, bottom darker)
      juce::ColourGradient panG(c1, r.getCentreX(), r.getY(), c2,
                                r.getCentreX(), r.getBottom(), false);
      panG.addColour(0.3f,
                     juce::Colour::fromRGB(
                         (int)(82 * darkenFactor), (int)(68 * darkenFactor),
                         (int)(55 * darkenFactor))); // Reduced top highlight

      g.setGradientFill(panG);
      g.fillRoundedRectangle(r, curRadius);

      // Apply subtle color temperature tint (studio lighting feel)
      if (!colorTint.isTransparent()) {
        g.setColour(colorTint.withAlpha(0.04f)); // Extremely subtle
        g.fillRoundedRectangle(r, curRadius);
      }

      // Subtle surface texture variations (5% difference)
      if (surfaceType == 0) {
        // Brushed metal (AMP & COMP) - horizontal lines
        for (float yy = r.getY(); yy < r.getBottom(); yy += 3.0f) {
          g.setColour(juce::Colours::white.withAlpha(0.015f));
          g.drawLine(r.getX(), yy, r.getRight(), yy, 0.5f);
        }
      } else if (surfaceType == 1) {
        // Dark anodized (FX) - subtle matte texture
        for (int i = 0; i < 50; ++i) {
          float rx = r.getX() +
                     juce::Random::getSystemRandom().nextFloat() * r.getWidth();
          float ry = r.getY() + juce::Random::getSystemRandom().nextFloat() *
                                    r.getHeight();
          g.setColour(juce::Colours::black.withAlpha(0.02f));
          g.fillRect(rx, ry, 1.5f, 1.5f);
        }
      } else if (surfaceType == 2) {
        // --- POLISHED BRONZE/BROWN (Master Out) - Subtle polish, mostly dark
        // Much darker, only slightly lighter than base panels
        juce::ColourGradient polish(juce::Colour(0xff5a4d40), r.getX(),
                                    r.getY(), juce::Colour(0xff2d241d),
                                    r.getRight(), r.getBottom(), false);
        polish.addColour(0.3f, juce::Colour(0xff6a5848)); // Subtle highlight
        polish.addColour(0.5f, juce::Colour(0xff453a2e)); // Mid-tone
        polish.addColour(0.7f, juce::Colour(0xff524638)); // Lower reflection
        g.setGradientFill(polish);
        g.fillRoundedRectangle(r, curRadius);

        // Very subtle horizontal polish streak
        juce::Path streak;
        streak.addRectangle(r.getX(), r.getCentreY() - 2.0f, r.getWidth(),
                            4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f)); // Much more subtle
        g.fillPath(streak);

        // Minimal warm highlight rim
        g.setColour(juce::Colour(0xff7a6850).withAlpha(0.15f)); // Very subtle
        g.drawRoundedRectangle(r.reduced(0.5f), curRadius, 1.5f);
      }

      // Subtle Inner Shadow for Depth (Modulated by bias)
      float innerShadowAlpha = (depthBias < 0.0f) ? 0.45f : 0.25f;
      for (float i = 0.5f; i <= 3.5f; i += 1.0f) {
        g.setColour(juce::Colours::black.withAlpha(innerShadowAlpha / i));
        g.drawRoundedRectangle(r.reduced(i), curRadius, 1.0f);
      }

      // 2. DOUBLE BLACK FRAME (Groove)
      // Outer groove line
      g.setColour(juce::Colours::black.withAlpha(0.9f));
      g.drawRoundedRectangle(r, curRadius, 1.5f);

      // Inner groove line (slightly inset)
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.drawRoundedRectangle(r.reduced(2.5f), curRadius, 1.0f);

      // Highlight between the double lines (The Ridge) - Darkened by 15% &
      // Desaturated
      g.setColour(juce::Colour(0xff7a756a)
                      .darker(0.15f)
                      .withMultipliedSaturation(0.92f)
                      .withAlpha(0.6f));
      g.drawRoundedRectangle(r.reduced(1.5f), curRadius, 1.5f);

      g.restoreState();
    }

    // --- INSANE 3D HYPER-INDUSTRIAL BEZEL ---

    // 1. Foundation Gap (Where the frame sits on tolex/plate)
    // A sharp dark groove all around to ground it
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(r.expanded(1.5f), curRadius, 1.5f);

    // 2. Ambient Occlusion (Soft layered shadow for weight - Modulated by bias)
    float aoBase =
        (depthBias > 0.0f) ? 0.65f : ((depthBias < 0.0f) ? 0.15f : 0.45f);
    g.setColour(juce::Colours::black.withAlpha(aoBase));
    for (float i = 1.0f; i <= 8.0f; i += 2.0f) {
      float ext = (depthBias > 0.0f) ? (i * 0.6f) : (i * 0.4f);
      g.drawRoundedRectangle(r.expanded(ext).translated(i, i), curRadius, 2.5f);
    }

    // 3. Sculpted Frame Body (12px Heavy Metal)
    // Bottom/Shadow side (Deep metal)
    g.setColour(juce::Colour(0xff1a1816));
    g.drawRoundedRectangle(r.translated(1.5f, 1.5f), curRadius, 12.0f);
    // Top/Light side (Industrial Bronze highlight) - Darkened by 15% &
    // Desaturated
    float hiMod =
        (depthBias > 0.0f) ? 0.15f : ((depthBias < 0.0f) ? -0.25f : 0.0f);
    g.setColour(juce::Colour(0xff5a554a)
                    .darker(0.15f - hiMod)
                    .withMultipliedSaturation(0.92f));
    g.drawRoundedRectangle(r.translated(-0.8f, -0.8f), curRadius, 12.0f);

    // 4. Anodized Accent Layer (The "tint") - Darkened by 15%
    g.setColour(baseCol.darker(0.15f).withAlpha(0.28f));
    g.drawRoundedRectangle(r, curRadius, 10.0f);

    // 5. Polished Ridge (The sharp peak edge) - Desaturated
    g.setColour(
        baseCol.brighter(0.4f).withMultipliedSaturation(0.92f).withAlpha(0.5f));
    g.drawRoundedRectangle(r.reduced(0.5f), curRadius, 1.2f);

    // 6. Specular Sparkle (Top-left extreme catching light)
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(r.reduced(0.2f).translated(-1.5f, -1.5f), curRadius,
                           0.7f);

    // 7. Extreme Inner Cut Cave (Panel depth)
    // This makes the panel seem deeply recessed inside the frame
    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.drawRoundedRectangle(r.reduced(6.0f), curRadius, 4.0f);

    // 8. Bottom Rim Reflection (Light from the ground)
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(r.reduced(0.5f).translated(1.8f, 1.8f), curRadius,
                           0.4f);
  };
  // Sections and Titles below
  // --- 3D Screwed-On Badge ---
  {
    auto badge = L.topBar.reduced(16.0f);
    float cr = 6.0f; // Corner radius for badge

    g.saveState();
    // Badge Background (Darker/Different Metal)
    juce::Colour b1 = juce::Colour(0xff2d2926); // Dark Ash
    juce::Colour b2 = juce::Colour(0xff1a1816);
    juce::ColourGradient bg(b1, badge.getX(), badge.getY(), b2,
                            badge.getRight(), badge.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(badge, cr);

    // Fine texture for badge
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    for (int i = 0; i < 100; ++i) {
      float rx = badge.getX() +
                 juce::Random::getSystemRandom().nextFloat() * badge.getWidth();
      float ry = badge.getY() + juce::Random::getSystemRandom().nextFloat() *
                                    badge.getHeight();
      g.fillRect(rx, ry, 2.0f, 2.0f);
    }

    // Badge Border (Bevel) - Desaturated Gold/Bronze
    g.setColour(juce::Colour(0xff605040)
                    .withMultipliedSaturation(0.92f)); // Bronze Highlight
    g.drawRoundedRectangle(badge.expanded(0.5f), cr, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(badge.reduced(1.0f), cr, 1.0f);

    // 4 Small Screws for the Badge
    auto drawSmallScrew = [&](float x, float y) {
      float s = 10.0f;
      juce::Rectangle<float> sr(x - s / 2, y - s / 2, s, s);
      g.setColour(juce::Colours::black.withAlpha(0.8f));
      g.fillEllipse(sr.translated(1, 1));

      juce::ColourGradient sg(juce::Colour(0xffc0c0c0), sr.getX(), sr.getY(),
                              juce::Colour(0xff404040), sr.getRight(),
                              sr.getBottom(), false);
      g.setGradientFill(sg);
      g.fillEllipse(sr);

      g.setColour(juce::Colours::black.withAlpha(0.7f));
      g.drawLine(sr.getX() + 2, sr.getY() + s / 2, sr.getRight() - 2,
                 sr.getBottom() - s / 2, 1.5f);
    };

    float pad = 8.0f;
    drawSmallScrew(badge.getX() + pad, badge.getY() + pad);
    drawSmallScrew(badge.getRight() - pad, badge.getY() + pad);
    drawSmallScrew(badge.getX() + pad, badge.getBottom() - pad);
    drawSmallScrew(badge.getRight() - pad, badge.getBottom() - pad);

    // --- KUNTERBUNT TEXT ---
    // "FUNKY MOOSE BASS STRATEGY"
    // We draw char by char to color them individually.

    juce::String fullText = "FUNKY MOOSE  BASS STRATEGY";
    juce::Font mainFont(
        juce::FontOptions("CartoonVibes", 45.0f, juce::Font::plain));
    juce::Font subFont(
        juce::FontOptions("CartoonVibes", 37.5f, juce::Font::plain));
    subFont.setHorizontalScale(0.94f); // minimal thinner

    float extraSpacing = 2.0f; // Tighter spacing

    // Calculate total width with extra spacing to center strictly
    float totalW = 0.0f;
    for (int i = 0; i < fullText.length(); ++i) {
      auto f = (i < 12) ? mainFont : subFont;
      totalW += f.getStringWidthFloat(fullText.substring(i, i + 1));
      if (i < fullText.length() - 1)
        totalW += extraSpacing;
    }

    float startX = badge.getCentreX() - totalW / 2.0f;
    float curX = startX;

    for (int i = 0; i < fullText.length(); ++i) {
      juce::String charStr = fullText.substring(i, i + 1);
      auto f = (i < 12) ? mainFont : subFont;
      float charW = f.getStringWidthFloat(charStr);
      g.setFont(f);

      if (charStr.trim().isNotEmpty()) {
        juce::Colour c = juce::Colour(0xffe8e8e8); // Default Bright White

        if (i < 5)
          c = juce::Colour(0xff3377ff); // FUNKY -> Brighter Blue
        else if (i >= 6 && i <= 10)
          c = juce::Colour(0xffffdd00); // MOOSE -> Brighter Yellow

        // 1. Glow/Bloom behind - Reduced for BASS STRATEGY
        float glowAlpha = (i < 12) ? 0.15f : 0.05f;
        g.setColour(c.withAlpha(glowAlpha));
        g.drawText(charStr, (int)curX, (int)(badge.getY()), (int)charW + 10,
                   (int)badge.getHeight(), juce::Justification::centred, false);

        // 2. Drop Shadow (Deep)
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawText(charStr, (int)(curX + 3), (int)(badge.getY() + 3),
                   (int)charW + 4, (int)badge.getHeight(),
                   juce::Justification::centred, false);

        // 3. Main Colored Body (Solid color, no gradient)
        g.setColour(c);
        g.drawText(charStr, (int)curX, (int)(badge.getY()), (int)charW + 4,
                   (int)badge.getHeight(), juce::Justification::centred, false);

        // 4. Highlight/Rim (White outline simulation) - Stronger
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawText(charStr, (int)(curX - 1), (int)(badge.getY() - 1),
                   (int)charW + 4, (int)badge.getHeight(),
                   juce::Justification::centred, false);
      }

      curX += charW + extraSpacing;
    }

    g.restoreState();
  }

  // Section frames
  const auto darkFrameCol =
      juce::Colour(0xff121210); // Almost black, slightly warm

  // Subtle color temperature tints (studio lighting feel)
  const auto warmTint =
      juce::Colour::fromRGB(255, 220, 180); // Warm amber for AMP
  const auto coolTint =
      juce::Colour::fromRGB(180, 200, 220); // Cool blue for COMP
  const auto greenTint =
      juce::Colour::fromRGB(200, 220, 200); // Subtle green for FX

  // Meter / Master (Raised)
  drawFrame(L.meter, -1.0f, -1.0f, darkFrameCol, false, 0.6f);
  // AMP (Baseline with warm glow, brushed metal)
  drawFrame(L.amp, -1.0f, -1.0f, darkFrameCol, false, 0.0f, 1.0f, warmTint, 0);
  // COMP (Slightly darker with cool tone, brushed metal)
  drawFrame(L.comp, -1.0f, -1.0f, darkFrameCol, true, 0.0f, 0.92f, coolTint, 0);

  // FX Slots: Frames Only (Static)
  const float fxDarken = 0.975f;
  for (int i = 0; i < 4; ++i) {
    drawFrame(L.fxSlots[(size_t)i].reduced(4.0f), 10.0f, 1.2f, darkFrameCol,
              true, -0.4f, fxDarken, greenTint, 1); // Dark anodized FX
  }

  // Master / Output Meter (Polished silver/steel Frontplate)
  drawFrame(L.master, -1.0f, -1.0f, darkFrameCol, true, 0.4f, 1.1f,
            juce::Colours::transparentBlack, 2); // Polished master
  drawFrame(L.outVuArea, -1.0f, -1.0f, darkFrameCol, true, 0.4f, 1.1f,
            juce::Colours::transparentBlack, 2); // Polished output area
  drawFrame(L.elchArea, 0.0f, 1.5f, darkFrameCol, true, -0.2f);

  // Titles
  g.setColour(juce::Colour(0xffe8e8e8));

  auto titleAt = [&](juce::Rectangle<float> r, const juce::String &t,
                     bool hasFlowNum = false) {
    auto header = r.reduced(14.0f, 5.0f).removeFromTop(26.0f);
    if (hasFlowNum) {
      header.removeFromLeft(30.0f); // Reserve space for dynamic badge
    }
    drawLabel(g, header, t, 15.2f, juce::Justification::left);
  };

  titleAt(L.meter, "INPUT METER", true);
  titleAt(L.amp, "AMP", true);
  titleAt(L.comp, "COMPRESSOR", true);

  // FX module names (Leave space for badges 4-7)
  for (int i = 0; i < 4; ++i) {
    auto rr = L.fxSlots[(size_t)i].reduced(18.0f, 5.0f).removeFromTop(24.0f);
    rr.removeFromLeft(30.0f); // Reserve space
    drawLabel(g, rr, L.fxNames[(size_t)i], 13.8f, juce::Justification::left);
  }

  titleAt(L.master, "MASTER OUT / CAB", true);
  titleAt(L.outVuArea, "OUTPUT METER");

  // ELCH Title
  drawLabel(g,
            L.elchArea.reduced(14.0f, 5.0f)
                .removeFromTop(26.0f)
                .translated(75.0f, 12.0f),
            "ELCH / VISUAL / RMS", 15.2f, juce::Justification::left);

  // Knob labels
  auto labelUnder = [&](juce::Component &c, const juce::String &t,
                        float size = 14.0f,
                        juce::Colour col = juce::Colour(0xffe8e8e8)) {
    auto b = c.getBounds().toFloat();
    g.setColour(col);
    drawLabel(
        g,
        {b.getX() - 12.0f, b.getBottom() + 10.0f, b.getWidth() + 24.0f, 22.0f},
        t, size);
  };

  // AMP
  labelUnder(gainKnob, "GAIN");
  labelUnder(bassKnob, "BASS");
  labelUnder(midKnob, "MID");
  labelUnder(trebleKnob, "TREBLE");
  labelUnder(volumeKnob, "VOLUME");

  // SLAP label
  {
    // SLAP label - handled below
  }

  // TUBE & LOW CUT Section (Unified)
  {
    auto bLC = lowCutToggle.getBounds().toFloat();
    auto bT = tubeToggle.getBounds().toFloat();
    auto bS = slapToggle.getBounds().toFloat();

    // Unified Labeling for Amp Toggles - BELOW the switches
    drawLabel(g, {bLC.getX() - 10.0f, bLC.getBottom() + 1.0f, 100.0f, 14.0f},
              "LOW CUT", 10.5f, juce::Justification::centredTop);
    drawLabel(g, {bLC.getX() - 10.0f, bLC.getBottom() + 13.0f, 100.0f, 12.0f},
              "40 Hz", 10.0f, juce::Justification::centredTop);

    drawLabel(g, {bT.getX() - 10.0f, bT.getBottom() + 1.0f, 100.0f, 14.0f},
              "TUBE", 10.5f, juce::Justification::centredTop);

    drawLabel(g, {bS.getX() - 10.0f, bS.getBottom() + 1.0f, 100.0f, 14.0f},
              "SLAP", 10.5f, juce::Justification::centredTop);
    drawLabel(g, {bS.getX() - 10.0f, bS.getBottom() + 13.0f, 100.0f, 12.0f},
              "+5 dB @ 8k", 9.5f, juce::Justification::centredTop);
  }

  // COMP
  labelUnder(compInKnob, "INPUT", 13.0f);
  labelUnder(compThreshKnob, "THRESH", 13.0f);
  labelUnder(compMakeKnob, "MAKEUP", 13.0f);
  labelUnder(compAtkKnob, "ATTACK", 13.0f);
  labelUnder(compRelKnob, "RELEASE", 13.0f);

  g.setColour(juce::Colour(0xffe8e8e8));
  {
    auto b = ratioBox.getBounds().toFloat();
    drawLabel(g, {b.getX(), b.getBottom() + 8.0f, b.getWidth(), 18.0f}, "RATIO",
              12.0f);
  }

  // Helper for ON/OFF labels above toggles
  auto labelToggle = [&](juce::ToggleButton &t) {
    auto b = t.getBounds().toFloat();
    drawLabel(g,
              {b.getX() - 10.0f, b.getY() - 16.0f, b.getWidth() + 20.0f, 16.0f},
              "ON/OFF", 11.0f, juce::Justification::centredBottom);
  };

  labelToggle(ampOnToggle);
  labelToggle(compOn);
  labelToggle(masterOn);
  // Manual draw to move it higher (+2 instead of default +10)
  {
    auto b = mixKnob.getBounds().toFloat();
    g.setColour(juce::Colour(0xffe8e8e8).withAlpha(0.7f));
    drawLabel(
        g,
        {b.getX() - 12.0f, b.getBottom() + 2.0f, b.getWidth() + 24.0f, 22.0f},
        "DRY/WET", 9.5f);
  }

  // MONO MAKER label
  // MONO MAKER label (positioned below the knob like DRY/WET)
  {
    auto bK = monoMakerKnob.getBounds().toFloat();
    g.setColour(juce::Colour(0xffe8e8e8).withAlpha(0.7f));

    // Positioned below the knob
    drawLabel(g,
              {bK.getX() - 15.0f, bK.getBottom() + 10.0f, bK.getWidth() + 30.0f,
               16.0f},
              "MONO MAKER", 11.0f, juce::Justification::centredTop);
  }

  // Labels for sub-toggles (Modern/Parallel/Auto)
  auto labelSubToggle = [&](juce::ToggleButton &t, const juce::String &txt) {
    auto b = t.getBounds().toFloat();
    drawLabel(g,
              {b.getX() - 15.0f, b.getY() - 16.0f, b.getWidth() + 30.0f, 16.0f},
              txt, 11.0f, juce::Justification::centredBottom);
  };

  // POSITIONED ABOVE for Master area (to clear screws/corner elements)
  auto labelSubToggleMaster = [&](juce::ToggleButton &t,
                                  const juce::String &txt) {
    auto b = t.getBounds().toFloat();
    drawLabel(g,
              {b.getX() - 15.0f, b.getY() - 15.0f, b.getWidth() + 30.0f, 16.0f},
              txt, 11.0f, juce::Justification::centredBottom);
  };

  labelSubToggle(octModernToggle, "MODERN");
  labelSubToggle(fxParallelToggle, "PARALLEL");
  labelSubToggleMaster(ampAutoGainToggle, "AUTO GAIN");
  labelSubToggleMaster(autoGateToggle, "AUTO GATE");
  labelSubToggleMaster(compAutoMakeupToggle, "AUTO GAIN");
  labelSubToggleMaster(autoGainToggle, "AUTO GAIN");
  labelSubToggleMaster(monoMakerToggle, "ON/OFF");

  // --- CAB Button Recessed Frame (Window Style) ---
  {
    auto b = cabButton.getBounds().toFloat();
    // Industrial recessed slot (The Frame)
    drawFrame(b.expanded(2.0f), 4.0f, 1.5f, juce::Colour(0xff080807), true);

    // Glass/Recessed Glow inside the window
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(b, 3.0f);

    // Inner Shadow line
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(b, 3.0f, 1.0f);

    // Cabinet Label above the slot
    drawLabel(g, {b.getX(), b.getY() - 18.0f, b.getWidth(), 14.0f}, "CABINET",
              10.0f);
  }

  // FX parameter labels
  g.setColour(sub);
  labelUnder(oct1Knob, "OCT 1", 12.0f, sub);
  labelUnder(oct2Knob, "OCT 2", 12.0f, sub);
  labelUnder(octMixKnob, "MIX", 12.0f, sub);

  labelToggle(octOn);
  labelToggle(envOn);
  labelToggle(phaserOn);
  labelToggle(chorusOn);

  labelUnder(envAtkKnob, "ATTACK", 12.0f, sub);
  labelUnder(envDecKnob, "DECAY", 12.0f, sub);
  labelUnder(envRangeKnob, "RANGE", 12.0f, sub);

  labelUnder(phRateKnob, "RATE", 12.0f, sub);
  labelUnder(phColKnob, "COLOUR", 12.0f, sub);
  labelUnder(phMixKnob, "MIX", 12.0f, sub);

  labelUnder(chRateKnob, "RATE", 12.0f, sub);
  labelUnder(chDepthKnob, "DEPTH", 12.0f, sub);
  labelUnder(chMixKnob, "MIX", 12.0f, sub);

  // MASTER - Overlay
  // If a pre-rendered skin overlay exists, draw it; otherwise nothing.
  if (!cachedSkinOverlay.isNull()) {
    g.drawImage(cachedSkinOverlay, 0, 0, (float)designW, (float)designH, 0, 0,
                cachedSkinOverlay.getWidth(), cachedSkinOverlay.getHeight(),
                false);
  }

  // --- Module Mounting Screws (Rack-Mount Style) ---
  auto drawModScrew = [&](juce::Point<float> p) {
    float s = 13.0f;
    juce::Rectangle<float> sr(p.x - s / 2, p.y - s / 2, s, s);

    // Hole shadow
    g.setColour(juce::Colours::black.withAlpha(1.0f));
    g.fillEllipse(sr.translated(0.5f, 0.5f).expanded(0.5f));

    // Screw Head
    juce::ColourGradient sg(juce::Colour(0xff606060), sr.getX(), sr.getY(),
                            juce::Colour(0xff252525), sr.getRight(),
                            sr.getBottom(), false);
    g.setGradientFill(sg);
    g.fillEllipse(sr.reduced(0.5f));

    // --- Stronger Inner Shadow ---
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(sr.reduced(1.2f), 1.2f);

    // Cross Slot
    g.setColour(juce::Colours::black);
    g.fillRect(sr.reduced(3.5f, 5.0f));
    g.fillRect(sr.reduced(5.0f, 3.5f));

    // Tiny rim highlight
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawEllipse(sr.reduced(0.5f), 0.8f);
  };

  auto addScrews = [&](juce::Rectangle<float> r, float inset) {
    drawModScrew(r.getTopLeft().translated(inset, inset));
    drawModScrew(r.getTopRight().translated(-inset, inset));
    drawModScrew(r.getBottomLeft().translated(inset, -inset));
    drawModScrew(r.getBottomRight().translated(-inset, -inset));
  };

  // Apply screws
  float modInset = 6.0f;
  addScrews(L.meter, modInset);
  addScrews(L.amp, modInset);
  addScrews(L.comp, modInset);
  addScrews(L.master, modInset);
  addScrews(L.outVuArea, modInset);

  for (size_t i = 0; i < 4; ++i) {
    addScrews(L.fxSlots[i], 5.0f);
  }
}

void FunkyMooseAudioProcessorEditor::updateGlowCaches() {
  auto L = getLayout();

  // Master Glow (Soft bloom)
  {
    float size = 120.0f;
    cachedMasterGlow =
        juce::Image(juce::Image::ARGB, (int)size, (int)size, true);
    juce::Graphics ig(cachedMasterGlow);
    juce::Rectangle<float> r(0, 0, size, size);
    juce::Point<float> center(size / 2.0f, size / 2.0f);

    for (float expand = 6.0f; expand <= 28.0f; expand += 5.5f) {
      float alpha = 1.0f / (expand * 0.35f);
      ig.setColour(juce::Colours::white.withAlpha(alpha));
      ig.fillEllipse(juce::Rectangle<float>(0, 0, expand * 2.0f, expand * 2.0f)
                         .withCentre(center));
    }
  }

  // FX Glow (Soft rounded rect)
  {
    auto fxRect = L.fxSlots[0].reduced(4.0f);
    float fxW = fxRect.getWidth();
    float fxH = fxRect.getHeight();
    float margin = 12.0f;
    cachedFxGlow = juce::Image(juce::Image::ARGB, (int)(fxW + margin * 2.0f),
                               (int)(fxH + margin * 2.0f), true);
    juce::Graphics ig(cachedFxGlow);
    juce::Rectangle<float> r(margin, margin, fxW, fxH);

    for (float expand = 1.0f; expand <= 6.0f; expand += 1.5f) {
      float alpha = 1.0f / expand;
      ig.setColour(juce::Colours::white.withAlpha(alpha));
      ig.drawRoundedRectangle(r.expanded(expand), 10.0f + expand * 0.5f, 1.5f);
    }
  }
}

void FunkyMooseAudioProcessorEditor::paintContent(juce::Graphics &g) {
  // 1. Draw Cached Static Background (Zero CPU)
  if (cachedContentBackground.isValid())
    g.drawImageAt(cachedContentBackground, 0, 0);
  else
    updateStaticBackground(); // Fallback regeneration if missing

  // 2. Draw Dynamic Elements (Animations & Status)
  const float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
  const auto L = getLayout();

  // --- SIGNAL FLOW STATUS BADGES (1-8) ---
  auto drawStatusBadge = [&](juce::Rectangle<float> r, int num, bool active) {
    auto header = r.reduced(14.0f, 5.0f).removeFromTop(26.0f);
    auto badge = header.removeFromLeft(20.0f)
                     .withSize(20.0f, 20.0f)
                     .translated(-2.0f, 2.0f);

    // 1. Badge Background (Physical Effect)
    if (active) {
      // Glowing Active State
      auto glowCol = currentPalette.accent.brighter(0.2f);
      float pulse = 0.85f + 0.15f * std::sin(time * 3.0f + (float)num * 0.5f);

      // Outer Glow
      g.setColour(glowCol.withAlpha(0.2f * pulse));
      g.fillEllipse(badge.expanded(2.0f));

      // Main Body
      g.setColour(glowCol);
      g.fillEllipse(badge);

      // Text (Contrast)
      g.setColour(juce::Colours::black.withAlpha(0.8f));
    } else {
      // Recessed Bypass State
      g.setColour(juce::Colours::black.withAlpha(0.3f));
      g.fillEllipse(badge);
      g.setColour(juce::Colours::white.withAlpha(0.1f));
      g.drawEllipse(badge, 1.0f);

      // Text (Dimmed)
      g.setColour(juce::Colours::white.withAlpha(0.3f));
    }

    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText(juce::String(num), badge.toNearestInt(),
               juce::Justification::centred);
  };

  drawStatusBadge(L.meter, 1, true); // Input always "on"
  drawStatusBadge(L.amp, 2, ampOnToggle.getToggleState());
  drawStatusBadge(L.comp, 3, compOn.getToggleState());
  drawStatusBadge(L.fxSlots[0], 4, octOn.getToggleState());
  drawStatusBadge(L.fxSlots[1], 5, envOn.getToggleState());
  drawStatusBadge(L.fxSlots[2], 6, phaserOn.getToggleState());
  drawStatusBadge(L.fxSlots[3], 7, chorusOn.getToggleState());
  drawStatusBadge(L.master, 8, masterOn.getToggleState());

  // --- MASTER OUT KNOB GLOW (Output Level Feedback) ---
  {
    auto bOut = outKnob.getBounds().toFloat();
    if (masterOn.getToggleState()) {
      if (cachedMasterGlow.isNull())
        updateGlowCaches();

      // Dynamic glow based on output level
      float outLevel = outVu.level; // Use actual output meter level
      float pulse = 0.5f + 0.5f * std::sin(time * 3.5f);
      float baseAlpha = 0.35f + 0.4f * outLevel + 0.1f * pulse;

      juce::Colour glowCol =
          juce::Colour(0xff44ccff); // Cool cyan/blue for output

      // Draw pre-rendered bloom
      g.setColour(glowCol.withAlpha(baseAlpha));
      g.drawImage(cachedMasterGlow,
                  (int)(bOut.getCentreX() - cachedMasterGlow.getWidth() / 2),
                  (int)(bOut.getCentreY() - cachedMasterGlow.getHeight() / 2),
                  cachedMasterGlow.getWidth(), cachedMasterGlow.getHeight(), 0,
                  0, cachedMasterGlow.getWidth(), cachedMasterGlow.getHeight(),
                  true); // TINT with current color

      // Sharp inner ring
      g.setColour(glowCol.withAlpha(0.6f * baseAlpha));
      g.drawEllipse(bOut.expanded(2.5f), 2.8f);
    }
  }

  // FX Slots: Pulsing Outer Glows
  const juce::ToggleButton *fxToggles[] = {&octOn, &envOn, &phaserOn,
                                           &chorusOn};

  for (int i = 0; i < 4; ++i) {
    const bool isActive = fxToggles[i]->getToggleState();
    if (isActive) {
      if (cachedFxGlow.isNull())
        updateGlowCaches();

      auto col = currentPalette.accent.withRotatedHue((float)i * 0.05f)
                     .brighter((float)i * 0.1f - 0.15f);
      const float pulse = 0.5f + 0.5f * std::sin(time * 2.0f + (float)i * 0.5f);
      const float glowAlpha = 0.15f + 0.25f * pulse;

      float margin = 12.0f;
      auto r = L.fxSlots[(size_t)i].reduced(4.0f);

      g.setColour(col.withAlpha(glowAlpha));
      g.drawImage(cachedFxGlow, (int)(r.getX() - margin),
                  (int)(r.getY() - margin), cachedFxGlow.getWidth(),
                  cachedFxGlow.getHeight(), 0, 0, cachedFxGlow.getWidth(),
                  cachedFxGlow.getHeight(),
                  true); // TINT with current color
    }
  }

  // --- TUBE SAT LAMP (Between GAIN and BASS) ---
  {
    auto bG = gainKnob.getBounds().toFloat();
    auto bB = bassKnob.getBounds().toFloat();

    // Center the lamp in the first gap, same height as toggles
    float lx = (bG.getRight() + bB.getX()) / 2.0f;
    float ly = lowCutToggle.getBounds().getCentreY();
    float s = 14.0f;
    juce::Rectangle<float> r(lx - s / 2, ly - s / 2, s, s);

    // More sensitive mapping for visibility and color shift
    float vis = juce::jlimit(0.0f, 1.0f, tubeSatVisual * 3.0f);
    float colorShift = juce::jlimit(0.0f, 1.0f, tubeSatVisual * 5.0f);

    // Color: Yellow -> Cyan
    juce::Colour lampCol =
        juce::Colour(0xfff0e040)
            .interpolatedWith(juce::Colour(0xff00ffff), colorShift);

    if (vis > 0.02f) {
      // Stronger surrounding glow (Outer Glow)
      for (float expand = 2.0f; expand <= 12.0f; expand += 3.0f) {
        g.setColour(lampCol.withAlpha((0.3f * vis) / (expand / 2.0f)));
        g.fillEllipse(r.expanded(expand * vis));
      }

      g.setColour(lampCol.withAlpha(0.4f + 0.6f * vis));
      g.fillEllipse(r);

      // Spark/Highlight
      g.setColour(juce::Colours::white.withAlpha(0.8f * vis));
      g.fillEllipse(r.reduced(s * 0.35f));
    } else {
      g.setColour(juce::Colours::black.withAlpha(0.4f));
      g.fillEllipse(r);

      // Rim for the inactive glass
      g.setColour(juce::Colours::white.withAlpha(0.05f));
      g.drawEllipse(r, 1.0f);
    }
  }

  // --- SMART GATE Lamp & Toggle (top/left) ---
  {
    auto bG = gainKnob.getBounds().toFloat();
    auto bB = bassKnob.getBounds().toFloat();
    auto headerRect = L.amp.reduced(10.0f).withHeight(32.0f);

    // Position at Gap 1 (between Gain and Bass) - User's Plan A (blue dot)
    float x0 = bG.getCentreX();
    float step = bB.getCentreX() - bG.getCentreX();
    float lx = x0 + step * 0.5f; // Center of Gap 1

    // Height: Match header toggle baseline area
    float ly = headerRect.getY() + 24.0f;
    float s = 12.0f;
    juce::Rectangle<float> r(lx - s / 2.0f, ly - s / 2.0f, s, s);

    bool isOn = autoGateToggle.getToggleState();
    float activity = isOn ? smartGateVisual : 0.0f;
    juce::Colour gateCol = juce::Colour(0xffff00ff); // Magenta/Pink

    // diiode draw
    if (isOn) {
      if (activity < 0.05f) {
        g.setColour(juce::Colours::green.withAlpha(0.3f));
        g.fillEllipse(r);
      } else {
        for (float expand = 1.0f; expand <= 8.0f; expand += 2.5f) {
          g.setColour(gateCol.withAlpha((0.35f * activity) / (expand / 1.8f)));
          g.fillEllipse(r.expanded(expand * activity));
        }
        g.setColour(gateCol.withAlpha(0.5f + 0.5f * activity));
        g.fillEllipse(r);
        g.setColour(juce::Colours::white.withAlpha(0.7f * activity));
        g.fillEllipse(r.reduced(s * 0.35f));
      }
    } else {
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.fillEllipse(r);
      g.setColour(juce::Colours::white.withAlpha(0.1f));
      g.drawEllipse(r, 1.0f);
    }
  }
}

void FunkyMooseAudioProcessorEditor::resized() {
  auto area = getLocalBounds().toFloat();

  const float sx = area.getWidth() / (float)designW;
  const float sy = area.getHeight() / (float)designH;
  const float s = std::min(sx, sy);

  content.setTransform(juce::AffineTransform::scale(s));

  const int cw = (int)std::round(designW * s);
  const int ch = (int)std::round(designH * s);

  content.setTopLeftPosition((getWidth() - cw) / 2, (getHeight() - ch) / 2);

  content.setSize(designW, designH);
  overlayComp.setBounds(0, 0, designW, designH);
  elch.toFront(false);
}

void FunkyMooseAudioProcessorEditor::drawLabel(juce::Graphics &g,
                                               juce::Rectangle<float> r,
                                               const juce::String &text,
                                               float size,
                                               juce::Justification just) const {
  // Snap to pixel grid for crispness
  r = r.withPosition(std::round(r.getX()), std::round(r.getY()))
          .withSize(std::round(r.getWidth()), std::round(r.getHeight()));

  // Use a simple Font constructor for broader JUCE compatibility
  // Use a simple Font constructor for broader JUCE compatibility
  juce::Font font{juce::FontOptions(size)};
  g.setFont(font);

  // --- VINTAGE ENGRAVED EFFECT ---

  // 1. Bottom Rim Highlight (Catching light from above)
  g.setColour(juce::Colours::white.withAlpha(0.25f));
  g.drawFittedText(text, r.translated(0.0f, 1.0f).toNearestInt(), just, 1,
                   0.9f);

  // 2. Top Inner Shadow (Depth of the etching)
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.drawFittedText(text, r.translated(0.0f, -0.6f).toNearestInt(), just, 1,
                   0.9f);

  // 3. Main Text Body
  // Inactive or recessed look: Slightly off-white/silver
  g.setColour(juce::Colour(0xffe0e0e0));
  g.drawFittedText(text, r.toNearestInt(), just, 1, 0.9f);
}

FunkyMooseAudioProcessorEditor::LayoutRects
FunkyMooseAudioProcessorEditor::getLayout() const {
  LayoutRects L;

  // Adjusted margin to fit back into the frame overlay
  // "noch kleiner" -> Increase margin further.
  const float OM = 130.0f;
  const float M = 42.0f;
  const float G = 16.0f;
  const float topBarH = 54.0f;

  L.plate = juce::Rectangle<float>(OM, OM, (float)designW - 2.0f * OM,
                                   (float)designH - 2.0f * OM);

  auto content = L.plate.reduced(M);

  // 1. Top Row (Input Meter | Title)
  const float hTop = std::floor(content.getHeight() * 0.13f);
  auto topRow = content.removeFromTop(hTop);
  content.removeFromTop(G);

  L.meter = topRow.removeFromLeft(std::floor(topRow.getWidth() * 0.35f));
  topRow.removeFromLeft(G);
  L.topBar = topRow;

  // 2. Bottom Row (Output Meter | Master Knob) - INCREASED HEIGHT
  const float wRight =
      std::floor(content.getWidth() * 0.40f); // Match Elch Width

  const float hBottom = std::floor(content.getHeight() * 0.17f);
  auto bottomRow = content.removeFromBottom(hBottom);
  content.removeFromBottom(G);

  // Align Master with Elch (Right Column)
  L.master = bottomRow.removeFromRight(wRight);
  bottomRow.removeFromRight(G);
  L.outVuArea = bottomRow; // Rest is VU

  // 3. Center Row (Left: Controls, Right: Elch)
  // Right Col width ~40%
  // Right Col width ~40%
  // wRight already defined above
  L.elchArea = content.removeFromRight(wRight);
  content.removeFromRight(G);

  auto leftCol = content;

  // Split Left Col into Amp|Comp (Top) and FX (Bottom)
  // Increased slightly to 0.37f to fix Slap button clipping
  const float hUpper = std::floor(leftCol.getHeight() * 0.37f);
  auto upperRow = leftCol.removeFromTop(hUpper);
  leftCol.removeFromTop(G);
  auto lowerRow = leftCol;

  // Upper: Amp (55%) | Comp (45%)
  L.amp = upperRow.removeFromLeft(std::floor(upperRow.getWidth() * 0.55f));
  upperRow.removeFromLeft(G);
  L.comp = upperRow;

  // Lower: FX Grid 2x2
  L.fx = lowerRow; // Title label for FX section
  // FX Title height reserved inside paint? No, getLayout defines areas.
  // We can just utilize the area for the grid.
  // Let's create uniform 2x2
  // We want a title strip? "FX SECTION"
  const float hTitle = 24.0f;
  // L.fx is the container.
  auto grid = lowerRow;
  grid.removeFromTop(hTitle); // reserve space for title text drawing

  const float cellW = std::floor((grid.getWidth() - G) / 2.0f);
  const float cellH = std::floor((grid.getHeight() - G) / 2.0f);

  auto r1 = grid.removeFromTop(cellH);
  grid.removeFromTop(G);
  auto r2 = grid;

  L.fxSlots[0] = r1.removeFromLeft(cellW);
  r1.removeFromLeft(G);
  L.fxSlots[1] = r1;
  L.fxSlots[2] = r2.removeFromLeft(cellW);
  r2.removeFromLeft(G);
  L.fxSlots[3] = r2;

  return L;
}

void FunkyMooseAudioProcessorEditor::layoutContent() {
  const auto L = getLayout();

  const float P = 18.0f;       // Padding
  const float headerH = 36.0f; // Header Height for modules

  // --- 1. GLOBAL HEADER (Tolex Area) ---
  // The user wants these REALLY "ganz oben" in the frame/tolex, totally
  // clear of the plate. designW is 2048.

  // High up in the frame area
  float topY = 46.0f; // Adjusted to align with horizontal lines "einrasten"
  float rightX = (float)designW - 140.0f; // Margin

  // Fold Button
  openFolderButton.setBounds((int)(rightX - 50.0f), (int)topY, 50, 24);
  rightX -= 55.0f;

  // Save Button
  savePresetButton.setBounds((int)(rightX - 50.0f), (int)topY, 50, 24);
  rightX -= 55.0f;

  // Preset Selector
  presetSelector.setBounds((int)(rightX - 220.0f), (int)topY, 220, 24);

  // New: Tooltip Toggle (Left of Preset Selector)
  // presetSelector.getX() is approx rightX - 220.
  // We place Toggle 105px to the LEFT of that (width 100).
  toggleTooltips.setBounds(presetSelector.getX() - 105, (int)topY, 100, 24);

  // Stats HUD (Left of Values)
  statsHUD.setBounds(toggleTooltips.getX() - 170, (int)topY, 160, 24);

  // Ensure visibility
  presetSelector.toFront(false);
  savePresetButton.toFront(false);
  openFolderButton.toFront(false);
  toggleTooltips.toFront(false);
  statsHUD.toFront(false); // Move to front!

  // LOGO
  // g.drawImageWithin(logo... handled in paintContent)

  // --- 2. MODULE LAYOUT (Inside Plate) ---
  auto inner = [&](juce::Rectangle<float> r) {
    r = r.reduced(P);
    r.removeFromTop(headerH); // Skip module title space
    return r;
  };

  layoutAmp(inner(L.amp));
  layoutComp(inner(L.comp));
  layoutFx(inner(L.fx));
  layoutMaster(inner(L.master));

  // --- 3. METERING & ELCH ---
  // Slimmer Input Meter (Centred Vertically)
  auto inRect = L.meter.reduced(16.0f);
  auto inVuBounds =
      inRect.withSizeKeepingCentre(inRect.getWidth(), 32.0f).toNearestInt();
  inVu.setBounds(inVuBounds);
  tunerOverlay->setBounds(inVuBounds);

  // Tuner Toggle placement: In the top right corner of the panel area
  tunerToggle.setBounds(L.meter.getRight() - 70, L.meter.getY() + 10, 60, 20);

  // Slimmer Output Meter (Centred Vertically)
  auto outRect = L.outVuArea.reduced(20.0f); // More margin
  outVu.setBounds(
      outRect.withSizeKeepingCentre(outRect.getWidth(), 32.0f).toNearestInt());

  elch.setBounds(L.elchArea.reduced(16.0f).toNearestInt());

  // --- 4. TOGGLES (Small Lamps) ---
  auto placeToggle = [&](juce::ToggleButton &t, juce::Rectangle<float> frame) {
    // Standardized padding "ins eck"
    auto headerRect = frame.reduced(10.0f).withHeight(headerH);
    auto toggleRect = headerRect.removeFromRight(44.0f).reduced(0, 4.0f);
    // Move down slightly to clear frames
    t.setBounds(toggleRect.translated(0, 6.0f).toNearestInt());
  };

  placeToggle(compOn, L.comp);
  placeToggle(ampOnToggle, L.amp);
  placeToggle(masterOn, L.master);

  // Auto Gain: Perfectly aligned under Master ON/OFF
  {
    auto foot = L.master.reduced(10.0f).removeFromBottom(headerH);
    auto toggleRectBottom =
        foot.removeFromRight(44.0f).reduced(0, 4.0f).translated(0, -6.0f);
    autoGainToggle.setBounds(toggleRectBottom.toNearestInt());
  }

  // FX toggles per slot
  for (int i = 0; i < 4; ++i) {
    auto frame = L.fxSlots[(size_t)i];
    // Use headerH for consistent toggle size top and bottom
    auto head = frame.reduced(10.0f).removeFromTop(headerH);
    auto toggleRectTop =
        head.removeFromRight(44.0f).reduced(0, 4.0f).translated(0, 6.0f);

    // Bottom row for sub-toggles
    auto foot = frame.reduced(10.0f).removeFromBottom(headerH);
    auto toggleRectBottom =
        foot.removeFromRight(44.0f).reduced(0, 4.0f).translated(0, -6.0f);

    switch (i) {
    case 0:
      octOn.setBounds(toggleRectTop.toNearestInt());
      octModernToggle.setBounds(toggleRectBottom.toNearestInt());
      break;
    case 1:
      envOn.setBounds(toggleRectTop.toNearestInt());
      break;
    case 2:
      phaserOn.setBounds(toggleRectTop.toNearestInt());
      break;
    case 3:
      chorusOn.setBounds(toggleRectTop.toNearestInt());
      fxParallelToggle.setBounds(toggleRectBottom.toNearestInt());
      break;
    }
  }
}

void FunkyMooseAudioProcessorEditor::layoutAmp(
    const juce::Rectangle<float> &r) {
  constexpr float kPrimary = 92.0f;
  const float k = kPrimary;
  const float G = juce::jlimit(16.0f, 30.0f, (r.getWidth() - k * 5.0f) / 4.0f);

  // Work on a local copy (Rectangle::removeFromTop mutates the rectangle).
  auto rr = r;
  auto top = rr.removeFromTop(rr.getHeight() - 70.0f);
  auto knobRowW = k * 5.0f + G * 4.0f;
  auto row = top.withSizeKeepingCentre(knobRowW, k);

  auto x = row.getX();
  const float y = row.getY();
  for (auto *knob :
       {&gainKnob, &bassKnob, &midKnob, &trebleKnob, &volumeKnob}) {
    knob->setBounds((int)std::round(x), (int)std::round(y), (int)k, (int)k);
    x += k + G;
  }

  // AMP Toggles - Positioned at the very bottom of the module box
  auto bottomRow = rr.removeFromBottom(38.0f);
  float swW = 54.0f;
  float swH = 24.0f;
  float swY = bottomRow.getY();

  // Calculate gap centers (between knob centers)
  float x0 = row.getX() + k / 2.0f;
  float step = k + G;

  float gBassMid = x0 + step * 1.5f;
  float gMidTre = x0 + step * 2.5f;
  float gTreVol = x0 + step * 3.5f;

  // Gap 2 (Bass-Mid) -> LOW CUT
  lowCutToggle.setBounds((int)(gBassMid - swW / 2.0f), (int)swY, (int)swW,
                         (int)swH);

  // Gap 3 (Mid-Treble) -> TUBE
  tubeToggle.setBounds((int)(gMidTre - swW / 2.0f), (int)swY, (int)swW,
                       (int)swH);

  // Gap 4 (Treble-Volume) -> SLAP
  slapToggle.setBounds((int)(gTreVol - swW / 2.0f), (int)swY, (int)swW,
                       (int)swH);

  // Gap 2 (Bass-Mid) -> Smart Gate Toggle (Header Row) - User's Plan A
  {
    const auto &L = getLayout();
    auto headerRect = L.amp.reduced(10.0f).withHeight(32.0f);
    float gateW = swW; // Same as low cut
    float gateH = swH; // Same as low cut

    // Position toggle in the second gap (Bass-Mid) at header row height
    float gBassMid = x0 + step * 1.5f;
    auto toggleRect = juce::Rectangle<float>(
        gBassMid - gateW / 2.0f, headerRect.getCentreY() + 4.0f, gateW, gateH);
    autoGateToggle.setBounds(toggleRect.toNearestInt());
  }

  // Amp Auto Gain toggle - same height as ON/OFF, between Treble and Volume
  {
    const auto &L = getLayout();
    constexpr float headerH = 32.0f;
    constexpr float autoW = 44.0f;

    // Get the header area (same as ON/OFF toggle)
    auto headerRect = L.amp.reduced(10.0f).withHeight(headerH);

    // Calculate X position between Treble and Volume knobs
    float autoX = gTreVol; // Center between Treble and Volume

    // Create toggle rect at header height
    auto toggleRect = juce::Rectangle<float>(
        autoX - autoW / 2.0f, headerRect.getY() + 4.0f, autoW, 28.0f);
    toggleRect = toggleRect.translated(0, 6.0f); // Same offset as ON/OFF

    ampAutoGainToggle.setBounds(toggleRect.toNearestInt());
  }
}

void FunkyMooseAudioProcessorEditor::layoutComp(
    const juce::Rectangle<float> &r) {
  constexpr float kSecondary = 52.0f; // Even smaller (was 56)

  // User requested "2 row layout"
  // Row 1: Input | Threshold | Ratio
  // Row 2: Makeup | Attack | Release

  auto area = r.reduced(10.0f);

  // Reserve space for labels on the bottom row to prevent cutoff
  area.removeFromBottom(20.0f);

  const float gridHeight = area.getHeight() - 24.0f; // minimal padding
  const float rowH = gridHeight / 2.0f;
  const float wSlot = area.getWidth() / 3.0f;

  // Center alignment helper
  auto placeInSlot = [&](juce::Component &c, int row, int col, float size) {
    if (row < 0 || row > 1 || col < 0 || col > 2)
      return;

    float slotX = area.getX() + col * wSlot;
    // Massive vertical separation to reveal labels
    float spacingY = 44.0f;
    float slotY = area.getY() + row * rowH;

    // Shift Row 0 up and Row 1 down
    if (row == 0)
      slotY -= spacingY / 2.0f;
    if (row == 1)
      slotY += spacingY / 2.0f;

    // Center in slot
    float cx = slotX + wSlot * 0.5f;
    float cy = slotY + rowH * 0.5f;

    // For combo box (Ratio)
    if (&c == &ratioBox) {
      c.setBounds((int)(cx - 48.0f), (int)(cy - 12.0f), 96, 24);
      return;
    }

    // Standard square component (knobs)
    c.setBounds((int)(cx - size * 0.5f), (int)(cy - size * 0.5f), (int)size,
                (int)size);
  };

  const float k = kSecondary;

  // Row 1
  placeInSlot(compInKnob, 0, 0, k);
  placeInSlot(compThreshKnob, 0, 1, k);
  placeInSlot(ratioBox, 0, 2, k);

  // Extra: place punch button under ratio (Manual calc matching placeInSlot
  // logic)
  {
    float slotX = area.getX() + 2 * wSlot;
    // Apply same row 0 shift
    float spacingY = 44.0f;
    float slotY = area.getY() + 0 * rowH - spacingY / 2.0f;

    float cx = slotX + wSlot * 0.5f;
    float cy = slotY + rowH * 0.5f;
    // Position Punch button slightly below the Ratio box center (Match
    // ON/OFF switch height 28)
    punchButton.setBounds((int)(cx - 48.0f), (int)(cy + 18.0f), 96, 28);
  }

  // Row 2
  placeInSlot(compMakeKnob, 1, 0, k);
  placeInSlot(compAtkKnob, 1, 1, k);
  placeInSlot(compRelKnob, 1, 2, k);

  // Comp Auto Makeup toggle - bottom-right corner, same as ON/OFF toggle
  {
    const auto &L = getLayout();
    constexpr float headerH = 32.0f;
    auto foot = L.comp.reduced(10.0f).removeFromBottom(headerH);
    auto toggleRectBottom =
        foot.removeFromRight(44.0f).reduced(0, 4.0f).translated(0, -6.0f);
    compAutoMakeupToggle.setBounds(toggleRectBottom.toNearestInt());
  }
}

void FunkyMooseAudioProcessorEditor::layoutFx(const juce::Rectangle<float> &) {
  const auto L = getLayout();

  // Each FX module = title row + 3 knobs in a single horizontal row.
  // IMPORTANT: reserve enough space for the painted labels so they stay
  // INSIDE the card.
  constexpr float header = 34.0f;
  constexpr float insetX = 20.0f;
  constexpr float insetY = 14.0f;

  constexpr float k = 58.0f;            // FX knob diameter (smaller to fit)
  constexpr float gap = 24.0f;          // spacing between knobs
  constexpr float labelReserve = 44.0f; // minimal space for labels

  auto layout3 = [&](juce::Rectangle<float> area, juce::Component &a,
                     juce::Component &b, juce::Component &c) {
    area.removeFromTop(header);
    area = area.reduced(insetX, insetY);

    // Horizontal row of 3 knobs
    const float totalW = k * 3.0f + gap * 2.0f;
    float x = area.getCentreX() - totalW / 2.0f;

    // Place knobs CENTERED vertically in the FX box now that it is huge
    // "mittig" as requested (shifted UP further to account for labels)
    float y = area.getCentreY() - k / 2.0f - 12.0f;

    a.setBounds(juce::Rectangle<float>(x, y, k, k).toNearestInt());
    x += k + gap;
    b.setBounds(juce::Rectangle<float>(x, y, k, k).toNearestInt());
    x += k + gap;
    c.setBounds(juce::Rectangle<float>(x, y, k, k).toNearestInt());
  };

  layout3(L.fxSlots[0], oct1Knob, oct2Knob, octMixKnob);
  layout3(L.fxSlots[1], envAtkKnob, envDecKnob, envRangeKnob);
  layout3(L.fxSlots[2], phRateKnob, phColKnob, phMixKnob);
  layout3(L.fxSlots[3], chRateKnob, chDepthKnob, chMixKnob);
}

void FunkyMooseAudioProcessorEditor::layoutMaster(
    const juce::Rectangle<float> &r) {
  auto c = r.getCentre();
  c.y -= 19.0f; // Minimal tiefer (war -22)

  // Master Knob (Big)
  outKnob.setBounds(
      juce::Rectangle<float>(c.x - 45.0f, c.y - 55.0f, 110.0f, 110.0f)
          .toNearestInt());

  // Mix (Left of Master)
  mixKnob.setBounds(
      juce::Rectangle<float>(c.x - 135.0f, c.y - 25.0f, 50.0f, 50.0f)
          .toNearestInt());

  // Mono Maker (Further Left) - Toggle positioned BESIDE the knob
  float mmX = c.x - 225.0f;
  monoMakerKnob.setBounds(
      juce::Rectangle<float>(mmX, c.y - 24.0f, 48.0f, 48.0f).toNearestInt());

  // Toggle centered vertically with the knob, placed to the LEFT
  monoMakerToggle.setBounds(
      juce::Rectangle<float>(mmX - 34.0f, c.y - 12.0f, 24.0f, 24.0f)
          .toNearestInt());

  // Cab (Right of Master) - More distance now
  cabButton.setBounds(
      juce::Rectangle<float>(c.x + 105.0f, c.y - 13.0f, 86.0f, 26.0f)
          .toNearestInt());
}

void FunkyMooseAudioProcessorEditor::layoutElchArea(
    const juce::Rectangle<float> &r) {
  const float G = 18.0f;

  // Left: output VU meter box, Right: Elch
  auto area = r;
  const float wLeft = std::round(area.getWidth() * 0.42f);
  auto vuR = area.removeFromLeft(wLeft);
  area.removeFromLeft(G);
  auto elR = area;

  outVu.setBounds(vuR.reduced(6.0f).toNearestInt());
  elch.setBounds(elR.toNearestInt());
}

void FunkyMooseAudioProcessorEditor::timerCallback() {
  // Sync Tube Sat Indicator (Instant rise, slow fall)
  float peak = processor.getSaturationLevel();
  if (peak > tubeSatVisual)
    tubeSatVisual = peak;
  else
    tubeSatVisual *= 0.92f; // Slower decay for better visibility

  // Sync Smart Gate Activity
  float gateAct = processor.getGateActivity();
  if (gateAct > smartGateVisual)
    smartGateVisual = gateAct;
  else
    smartGateVisual *= 0.85f;

  // Check for Skin Change
  if (auto *raw = processor.apvts.getRawParameterValue("skin")) {
    int idx = (int)std::round(raw->load());
    if (idx != cachedSkinIndex) {
      updateStaticBackground();
      content.repaint();
    }
  }

  // Sync Cab Button Text
  if (auto *raw = processor.apvts.getRawParameterValue("cabType")) {
    int idx = (int)std::round(raw->load());
    juce::String txt =
        (idx == 0) ? "CAB: OFF" : ((idx == 1) ? "CAB: 4x10" : "CAB: 1x15");
    if (cabButton.getButtonText() != txt)
      cabButton.setButtonText(txt);
  }

  // Input RMS
  const float inRms = processor.getInRms();
  {
    const float inDb = juce::Decibels::gainToDecibels(inRms, -80.0f);
    const float inVu01 =
        juce::jmap(juce::jlimit(-60.0f, 0.0f, inDb), -60.0f, 0.0f, 0.0f, 1.0f);
    inVu.setLevel(inVu01);
  }

  // Output RMS
  const float outRms = processor.getOutRms();
  {
    const float outDb = juce::Decibels::gainToDecibels(outRms, -80.0f);
    const float outVu01 =
        juce::jmap(juce::jlimit(-60.0f, 0.0f, outDb), -60.0f, 0.0f, 0.0f, 1.0f);
    outVu.setLevel(outVu01);
  }

  // Elch: one unified call (internal smoothing, no flicker)
  const float grDb = processor.getCompGainReductionDb();
  const bool punchOn = processor.isPunchEnabledForUI();
  elch.setMooseState(inRms, outRms, grDb, punchOn);

  // Stats Update
  cpuUsage = processor.getCPUUsage();
  latencySamples = processor.getLatencySamples();
  statsHUD.update(cpuUsage, latencySamples);

  // repaint only the content canvas (keeps it snappy)
  content.repaint();
}

void FunkyMooseAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}
