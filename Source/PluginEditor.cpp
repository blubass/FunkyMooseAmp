#include "PluginEditor.h"
#include "BinaryData.h"

static void initKnob(juce::Slider &s) {
  s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  s.setRange(0.0, 1.0, 0.0);
  s.setSkewFactor(1.0);
}

FunkyMooseAudioProcessorEditor::FunkyMooseAudioProcessorEditor(
    FunkyMooseAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), processor(p), inVu(*this), outVu(*this),
      compGr(*this) {
  // 1. Initialize Palette and LookAndFeel FIRST
  const int skinIndex = 0;
  currentPalette = Skins::getPalette(skinIndex);
  setLookAndFeel(&lookAndFeel);
  lookAndFeel.setColors(currentPalette.accent, currentPalette.knob,
                        currentPalette.knobIndicator);

  inVu.meterLabel = "INPUT";
  outVu.meterLabel = "OUTPUT";
  compGr.meterLabel = "GR";

  compGr.setGRMode(true);
  compGr.setVertical(true);
  content.addAndMakeVisible(compGr);

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
    juce::PopupMenu m;
    m.addItem(1, "OFF");
    m.addItem(2, "4x10");
    m.addItem(3, "1x15");
    m.addSeparator();
    m.addItem(4, "User IR...");

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&cabButton),
                    [this](int result) {
                      if (result == 0)
                        return;

                      if (result == 4) {
                        openIrChooser();
                      } else {
                        int targetIdx = result - 1;
                        float val = (float)targetIdx /
                                    3.0f; // 4 choices total -> denon is 3
                        auto *p = processor.apvts.getParameter("cabType");
                        p->beginChangeGesture();
                        p->setValueNotifyingHost(val);
                        p->endChangeGesture();
                      }
                    });
  };
  content.addAndMakeVisible(cabButton);

  masterOn.setButtonText("");
  masterOn.setName("mainToggle");
  content.addAndMakeVisible(masterOn);

  tubeToggle.setButtonText("");
  content.addAndMakeVisible(tubeToggle);

  ampOnToggle.setButtonText("");
  ampOnToggle.setName("mainToggle");
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
  // The `masterOn` toggle is now handled separately above.
  for (auto *t : {&compOn, &octOn, &envOn, &phaserOn, &chorusOn,
                  &fxParallelToggle, &slapToggle}) {
    t->setButtonText("");
    t->setToggleState(t == &fxParallelToggle ? false : true,
                      juce::dontSendNotification);
    content.addAndMakeVisible(*t);
  }

  // Punch button (separate on purpose: it's a real button, not a tiny lamp)
  punchButton.setClickingTogglesState(true);
  punchButton.setName("punchButton");
  content.addAndMakeVisible(punchButton);

  // NEW: Tooltip Toggle
  content.addAndMakeVisible(toggleTooltips);
  toggleTooltips.setButtonText("Values");
  toggleTooltips.setName("tooltipToggle");
  toggleTooltips.setToggleState(true, juce::dontSendNotification);
  toggleTooltips.onClick = [this] {
    const bool show = toggleTooltips.getToggleState();
    for (auto *k : {&gainKnob,    &bassKnob,    &midKnob,        &trebleKnob,
                    &volumeKnob,  &compInKnob,  &compThreshKnob, &compMakeKnob,
                    &compAtkKnob, &compRelKnob, &oct1Knob,       &oct2Knob,
                    &octMixKnob,  &envAtkKnob,  &envDecKnob,     &envRangeKnob,
                    &phRateKnob,  &phColKnob,   &phMixKnob,      &chRateKnob,
                    &chDepthKnob, &chMixKnob,   &outKnob,        &monoMakerKnob,
                    &irMixKnob}) {
      k->setPopupDisplayEnabled(show, show, this);
    }
  };

  content.addAndMakeVisible(irMixKnob);

  autoGainToggle.setButtonText("");
  autoGainToggle.setName("autoGain");
  content.addAndMakeVisible(autoGainToggle);

  autoGateToggle.setButtonText("");
  content.addAndMakeVisible(autoGateToggle);

  compOn.setButtonText("");
  compOn.setName("mainToggle");
  content.addAndMakeVisible(compOn);

  monoMakerToggle.setButtonText("");
  content.addAndMakeVisible(monoMakerToggle);

  tunerToggle.setButtonText("TUNER");
  tunerToggle.setName("tunerToggle");
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

  content.addAndMakeVisible(midiIndicator);
  midiIndicator.setInterceptsMouseClicks(false, false);

  // NEW: Mono Input Toggle (Standalone focus)
  monoInputButton.setButtonText("MONO");
  monoInputButton.setName("monoInput"); // ID for LookAndFeel logic
  monoInputButton.setClickingTogglesState(true);

  if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone) {
    monoInputButton.setEnabled(false);
    monoInputButton.setAlpha(0.45f);
    monoInputButton.setTooltip(
        "MONO (Standalone Only): Mirrors Input 1 to L/R to avoid noise.");
  } else {
    monoInputButton.setTooltip("MONO: Mirrors Input 1 to L/R in Standalone to "
                               "avoid noise from open inputs.");
  }
  monoInputButton.setColour(juce::ToggleButton::tickColourId,
                            juce::Colours::transparentBlack);
  monoInputButton.setColour(juce::ToggleButton::textColourId,
                            juce::Colours::white);
  content.addAndMakeVisible(monoInputButton);

  if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone) {
    monoInputButton.setEnabled(false);
    monoInputButton.setAlpha(0.4f);
  }

  // PRESETS Header (Must be Added to Content!)
  content.addAndMakeVisible(presetSelector);
  presetSelector.setButtonText("Default");

  content.addAndMakeVisible(savePresetButton);
  content.addAndMakeVisible(openFolderButton);

  content.addAndMakeVisible(inVu);
  octOn.setName("mainToggle");
  envOn.setName("mainToggle");
  phaserOn.setName("mainToggle");
  chorusOn.setName("mainToggle");

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

  // Set L&F colors immediately
  elch.setColors(currentPalette.elchEye, currentPalette.elchGlow);
  elch.setBackgroundColor(currentPalette.accent.darker().withAlpha(0.2f));

  // Load Assets
  auto elchImg = juce::ImageCache::getFromMemory(
      BinaryData::elch_vintage_png, BinaryData::elch_vintage_pngSize);
  elch.setElchImage(elchImg);

  {
    auto rawFrame = juce::ImageCache::getFromMemory(
        BinaryData::FrameOverlay_png, BinaryData::FrameOverlay_pngSize);

    if (rawFrame.isValid()) {
      // Convert to ARGB so we can write alpha values
      juce::Image frameImg = rawFrame.convertedToFormat(juce::Image::ARGB);
      {
        juce::Image::BitmapData data(frameImg,
                                     juce::Image::BitmapData::readWrite);
        for (int y = 0; y < data.height; ++y) {
          for (int x = 0; x < data.width; ++x) {
            auto c = data.getPixelColour(x, y);
            // Make white/near-white interior pixels transparent
            // Keep dark frame pixels opaque
            if (c.getBrightness() > 0.85f) {
              data.setPixelColour(x, y, juce::Colours::transparentBlack);
            }
          }
        }
      }
      overlayComp.setImage(frameImg);
    } else {
      overlayComp.setImage(juce::Image());
    }
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
  // octModern attachment removed
  oct1Att = std::make_unique<SA>(ts, "oct1", oct1Knob);
  oct2Att = std::make_unique<SA>(ts, "oct2", oct2Knob);
  octMixAtt = std::make_unique<SA>(ts, "octMix", octMixKnob);

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
  irMixAtt = std::make_unique<SA>(ts, "irMix", irMixKnob);
  monoInputAtt = std::make_unique<BA>(ts, "forceMonoInput", monoInputButton);

  // Force 3 decimal places for popups (overriding attachment defaults)
  for (auto *k : {&gainKnob,      &bassKnob,    &midKnob,        &trebleKnob,
                  &volumeKnob,    &compInKnob,  &compThreshKnob, &compMakeKnob,
                  &compAtkKnob,   &compRelKnob, &oct1Knob,       &oct2Knob,
                  &octMixKnob,    &envAtkKnob,  &envDecKnob,     &envRangeKnob,
                  &phRateKnob,    &phColKnob,   &phMixKnob,      &chRateKnob,
                  &chDepthKnob,   &chMixKnob,   &outKnob,        &mixKnob,
                  &monoMakerKnob, &irMixKnob}) {
    k->textFromValueFunction = [](double value) {
      return juce::String(value, 1);
    };
  }

  updateStaticBackground();
  updateGlowCaches();

  // Allow the standalone window to be resized freely
  setResizable(true, true);
  setResizeLimits(800, 300, 4096, 2048);
  setSize(designW / 2, designH / 2); // Default: half of design size

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

    // --- ROAD WORN: GRIME & DIRT CLOUDS ---
    for (int i = 0; i < 400; ++i) {
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float s = 40.0f + rng.nextFloat() * 200.0f;
      float a = 0.05f + rng.nextFloat() * 0.12f;
      ig.setColour(
          juce::Colour(0xff15100a).withAlpha(a)); // Dark brown/black grime
      ig.fillEllipse(x - s / 2, y - s / 2, s, s);
    }

    // Layer 1: Heavy pitting/corrosion & Gouges (Dark)
    ig.setColour(juce::Colours::black.withAlpha(0.45f)); // Darker
    for (int i = 0; i < 5000; ++i) {                     // Way more
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float w = 2.0f + rng.nextFloat() * 40.0f;
      float h = 1.0f + rng.nextFloat() * 2.5f;
      ig.fillRect(x, y, w, h);
    }

    // Deep Gouges (with highlight edge)
    for (int i = 0; i < 300; ++i) {
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float w = 15.0f + rng.nextFloat() * 90.0f;
      float h = 1.5f + rng.nextFloat() * 2.0f;

      ig.setColour(juce::Colours::black.withAlpha(0.7f));
      ig.fillRect(x, y, w, h);

      ig.setColour(juce::Colours::white.withAlpha(0.12f));
      ig.fillRect(x, y + h, w, 0.8f); // Catch light inside the gouge
    }

    // Layer 2: Brass/Gold Scratches (Highlights)
    ig.setColour(
        juce::Colour::fromRGB(255, 200, 100).withAlpha(0.15f)); // Brighter
    for (int i = 0; i < 3500; ++i) {                            // Way more
      float y = plate.getY() + rng.nextFloat() * plate.getHeight();
      float x = plate.getX() + rng.nextFloat() * plate.getWidth();
      float w = 5.0f + rng.nextFloat() * 40.0f;
      float h = 0.8f;
      ig.fillRect(x, y, w, h);
    } // Close the 3500 loop here

    // --- TAPE RESIDUE AND SCRATCHY HAIRS ---
    // --- TAPE RESIDUE AND SCRATCHY HAIRS ---
    ig.setColour(
        juce::Colour(0xffe0ead5)
            .withAlpha(0.40f));   // Dull whitish residue - even higher opacity
    for (int i = 0; i < 4; ++i) { // 4 tape marks
      float tx = plate.getX() + rng.nextFloat() * (plate.getWidth() - 100);
      float ty = plate.getY() + rng.nextFloat() * (plate.getHeight() - 40);
      float tw = 60.0f + rng.nextFloat() * 60.0f;
      float th = 20.0f + rng.nextFloat() * 15.0f;

      juce::Path tapeBlock;
      tapeBlock.addRectangle(tx, ty, tw, th);
      tapeBlock.applyTransform(juce::AffineTransform::rotation(
          rng.nextFloat() * 0.4f - 0.2f, tx + tw / 2, ty + th / 2));
      ig.fillPath(tapeBlock);

      // glue edges
      ig.setColour(juce::Colour(0xff202020)
                       .withAlpha(0.60f)); // Dark grey sticky edge (no rust)
      ig.strokePath(tapeBlock, juce::PathStrokeType(2.5f));
    }

    // --- HEAVY BORDER AMBIENT OCCLUSION (Dust/Nicotine build up near tolex)
    // ---
    for (int layer = 0; layer < 5; ++layer) {
      float inset = layer * 4.0f;
      ig.setColour(
          juce::Colour(0xff151515)
              .withAlpha(
                  0.85f -
                  layer *
                      0.12f)); // Pure dark grey/black grime instead of brown
      ig.drawRect(plate.reduced(inset), 8.0f);
    }
  } // Close the cachedPlateTexture block

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

  // 0. PHYSICAL OUTER TOLEX ENCLOSURE (The "Amp Case")
  auto outerCase = area;
  g.setColour(juce::Colour(0xff080808));
  g.fillRect(outerCase);

  // Tolex Texture (Rough Leather feel)
  juce::Random rTolex(42);
  g.setColour(juce::Colours::white.withAlpha(0.015f));
  for (int i = 0; i < 2000; ++i) {
    g.fillRect(rTolex.nextFloat() * designW, rTolex.nextFloat() * designH, 1.5f,
               1.0f);
  }

  // Deep Corner Vignette for enclosure
  juce::ColourGradient caseV(juce::Colours::transparentBlack, area.getCentreX(),
                             area.getCentreY(),
                             juce::Colours::black.withAlpha(0.85f), 0, 0, true);
  g.setGradientFill(caseV);
  g.fillRect(area);

  // Subtle Ambient Glow behind the whole plate
  juce::ColourGradient ambientGlow(currentPalette.accent.withAlpha(0.06f),
                                   area.getCentreX(), area.getCentreY(),
                                   juce::Colours::transparentBlack, 0, 0, true);
  g.setGradientFill(ambientGlow);
  g.fillRect(area);

  // 1. RECESSED PLATE AREA
  auto plateArea = area.reduced(15.0f);
  g.setColour(currentPalette.background);
  g.fillRect(plateArea);

  // Inner Plate Bevel (Shadow inside the tolex)
  for (float i = 15.0f; i < 22.0f; i += 1.0f) {
    g.setColour(juce::Colours::black.withAlpha(0.6f / (i - 14.0f)));
    g.drawRect(area.reduced(i), 1.0f);
  }

  // Metal Texture & Gradients (ONLY ON PLATE)
  juce::ColourGradient metalGrad(
      currentPalette.background.brighter(0.1f), 0, plateArea.getY(),
      currentPalette.background.darker(0.1f), 0, plateArea.getBottom(), false);

  g.setGradientFill(metalGrad);
  g.fillRect(plateArea);

  juce::ColourGradient vignette(
      juce::Colours::transparentBlack, plateArea.getCentreX(),
      plateArea.getCentreY(),
      juce::Colours::black.withAlpha(0.75f), // Darker vignette for global depth
      plateArea.getX(), plateArea.getY(), true);

  g.setGradientFill(vignette);
  g.fillRect(plateArea);

  // Subtle noise & Studio Scanline
  g.saveState();
  g.setColour(juce::Colours::white.withAlpha(0.04f));
  for (int i = 0; i < designH; i += 2)
    g.fillRect(0, i, designW, 1);

  // GLOBAL LIGHTING SWIPE (Studio Beam)
  juce::Path swipe;
  swipe.addRectangle(0, 0, designW, designH);
  juce::ColourGradient beam(juce::Colours::white.withAlpha(0.06f), 0, 0,
                            juce::Colours::transparentWhite, designW * 0.4f,
                            designH * 0.4f, true);
  g.setGradientFill(beam);
  g.fillPath(swipe);
  g.restoreState();

  const auto L = getLayout();

  // Main plate - Heavy Brushed Industrial Metal
  {
    g.saveState();
    auto plate = L.plate;

    // 1. Base Metal: Physical Plate (Now dynamic via Palette)
    juce::Colour c1 = currentPalette.plate.brighter(0.18f);
    juce::Colour c2 = currentPalette.plate.darker(0.18f);

    juce::ColourGradient cg(c1, plate.getX(), plate.getY(), c2,
                            plate.getRight(), plate.getBottom(), false);

    // Dynamic highlights based on palette
    cg.addColour(0.3f, currentPalette.plate.brighter(0.24f));
    cg.addColour(0.7f, currentPalette.plate.darker(0.10f));

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
        // Draw cached overlay (which is full-screen size)
        g.drawImageAt(cachedPlateTexture, 0, 0, false);
      }
    }

    // 3. 3D Beveled Edge - Crisp Industrial
    float cr = currentPalette.cornerRadius;

    // INDUSTRIAL RIVETS (Extra Detail)
    auto drawRivet = [&](float rx, float ry) {
      float rs = 6.0f;
      juce::Rectangle<float> rr(rx - rs * 0.5f, ry - rs * 0.5f, rs, rs);
      g.setColour(juce::Colours::black.withAlpha(0.8f));
      g.fillEllipse(rr.translated(1, 1));
      juce::ColourGradient rg(juce::Colours::white.withAlpha(0.4f), rx, ry,
                              juce::Colours::black, rx + rs, ry + rs, true);
      g.setGradientFill(rg);
      g.fillEllipse(rr);
    };
    float rivM = 10.0f;
    drawRivet(plate.getX() + rivM, plate.getY() + plate.getHeight() * 0.5f);
    drawRivet(plate.getRight() - rivM, plate.getY() + plate.getHeight() * 0.5f);

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

      // Screw Body (Silver/Chrome) - Ampeg Style
      juce::ColourGradient sg(juce::Colour(0xffe0e0e0), sr.getX(), sr.getY(),
                              juce::Colour(0xff202020), sr.getRight(),
                              sr.getBottom(), false);
      sg.addColour(0.3f, juce::Colour(0xffffffff)); // Hot Specular
      g.setGradientFill(sg);
      g.fillEllipse(sr);

      // Radial polish distortion
      g.setColour(juce::Colours::white.withAlpha(0.3f));
      g.drawEllipse(sr.reduced(0.5f), 0.5f);

      // --- Stronger Inner Shadow for Depth ---
      g.setColour(juce::Colours::black.withAlpha(0.85f));
      g.drawEllipse(sr.reduced(1.0f), 2.5f);

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

      // 1. RECESSED PANEL (Now dynamic via Palette)
      juce::Colour c1 = currentPalette.panel.brighter(0.1f * darkenFactor);
      juce::Colour c2 = currentPalette.panel.darker(0.1f * darkenFactor);

      // Gradient: Smooth vertical/radial feel (top lighter, bottom darker)
      juce::ColourGradient panG(c1, r.getCentreX(), r.getY(), c2,
                                r.getCentreX(), r.getBottom(), false);
      panG.addColour(
          0.3f, juce::Colour::fromRGB(
                    (int)(60 * darkenFactor), (int)(62 * darkenFactor),
                    (int)(68 * darkenFactor))); // Steel highlight (Neutral)

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
        // Dark anodized (FX) - High-end sandblasted texture
        for (int i = 0; i < 200; ++i) {
          float rx = r.getX() +
                     juce::Random::getSystemRandom().nextFloat() * r.getWidth();
          float ry = r.getY() + juce::Random::getSystemRandom().nextFloat() *
                                    r.getHeight();
          float s = juce::Random::getSystemRandom().nextFloat() * 1.2f + 0.5f;
          g.setColour(juce::Colours::white.withAlpha(0.04f));
          g.fillRect(rx, ry, s, s);
          g.setColour(juce::Colours::black.withAlpha(0.06f));
          g.fillRect(rx + 0.5f, ry + 0.5f, s, s);
        }
      } else if (surfaceType == 2) {
        // --- POLISHED STEEL (Master Out) - Neutral Steel
        juce::ColourGradient polish(juce::Colour(0xff2a2a2a), r.getX(),
                                    r.getY(), juce::Colour(0xff0a0a0a),
                                    r.getRight(), r.getBottom(), false);
        polish.addColour(0.3f, juce::Colour(0xff4a4a4f)); // Steel highlight
        polish.addColour(0.5f, juce::Colour(0xff181818)); // Mid-tone
        polish.addColour(0.7f, juce::Colour(0xff25252a)); // Lower reflection
        g.setGradientFill(polish);
        g.fillRoundedRectangle(r, curRadius);

        // Very subtle horizontal polish streak
        juce::Path streak;
        streak.addRectangle(r.getX(), r.getCentreY() - 2.0f, r.getWidth(),
                            4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f)); // Much more subtle
        g.fillPath(streak);

        // Minimal neutral highlight rim
        g.setColour(juce::Colour(0xff707075).withAlpha(0.2f)); // Steel rim
        g.drawRoundedRectangle(r.reduced(0.5f), curRadius, 1.5f);
      }

      // --- HYPER-REALISTIC METER GLASS ---
      if (r == L.meter || r == L.outVuArea) {
        juce::Path glass;
        glass.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(),
                                  r.getHeight() * 0.45f, curRadius, curRadius,
                                  true, true, false, false);
        juce::ColourGradient gg(juce::Colours::white.withAlpha(0.15f), r.getX(),
                                r.getY(), juce::Colours::transparentWhite,
                                r.getX(), r.getBottom(), false);
        g.setGradientFill(gg);
        g.fillPath(glass);

        // Smudges & Fingerprints (Very subtle)
        juce::Random rnd(555);
        g.setColour(juce::Colours::black.withAlpha(0.03f));
        for (int i = 0; i < 4; ++i) {
          g.fillEllipse(r.getX() + rnd.nextFloat() * r.getWidth(),
                        r.getY() + rnd.nextFloat() * r.getHeight(), 35, 35);
        }

        // Edge light catching dust
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(r.reduced(0.8f), curRadius, 0.4f);
      }

      // (Hyper-realistic glass logic follows above)

      // Subtle Inner Shadow for Depth (Intensity ramped up for 3D)
      float innerShadowAlpha =
          (depthBias < 0.0f) ? 0.75f : 0.60f;      // Deeper inner shadow
      for (float i = 0.5f; i <= 6.5f; i += 1.0f) { // Wider spread
        g.setColour(juce::Colours::black.withAlpha(innerShadowAlpha / i));
        g.drawRoundedRectangle(r.reduced(i), curRadius, 1.0f);
      }

      // Grunge/Dust in corners
      g.setColour(juce::Colours::black.withAlpha(0.12f));
      g.fillEllipse(r.getX() - 2, r.getY() - 2, 10, 10);
      g.fillEllipse(r.getRight() - 8, r.getBottom() - 8, 10, 10);

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

    // 2. Ambient Occlusion (Soft layered shadow for weight - Modulated by
    // bias)
    float aoBase =
        (depthBias > 0.0f)
            ? 0.8f
            : ((depthBias < 0.0f) ? 0.25f
                                  : 0.6f); // Stronger shadows under modules
    g.setColour(juce::Colours::black.withAlpha(aoBase));
    for (int i = 1; i <= 10; i += 2) { // Increased spread
      float ext = (depthBias > 0.0f) ? (i * 0.7f) : (i * 0.5f);
      g.drawRoundedRectangle(r.expanded(ext).translated(i, i), curRadius,
                             3.0f); // Heavier shadow blur
    }

    // 3. Sculpted Frame Body (12px Heavy Metal) - Neutral Silver/Steel
    // Bottom/Shadow side (Deep metal)
    g.setColour(juce::Colour(0xff0a0a0a));
    g.drawRoundedRectangle(r.translated(1.5f, 1.5f), curRadius, 12.0f);
    // Top/Light side (Steel highlight)
    float hiMod =
        (depthBias > 0.0f) ? 0.15f : ((depthBias < 0.0f) ? -0.25f : 0.0f);
    g.setColour(juce::Colour(0xff808080).darker(0.15f - hiMod));
    g.drawRoundedRectangle(r.translated(-0.8f, -0.8f), curRadius, 12.0f);

    // 4. Anodized Accent Layer (The "tint")
    g.setColour(baseCol.darker(0.25f).withAlpha(0.35f));
    g.drawRoundedRectangle(r, curRadius, 10.0f);

    // Extra Bevel Layer for thickness
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(r.reduced(1.2f), curRadius, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(r.reduced(0.8f), curRadius, 0.5f);

    // 5. Polished Ridge (Sharp peak edge)
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(r.reduced(0.5f), curRadius, 0.8f);

    // 6. Specular Sparkle (Sharp industrial glint)
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawRoundedRectangle(r.reduced(0.2f).translated(-1.8f, -1.8f), curRadius,
                           0.6f);

    // 7. Extreme Inner Cut Cave (Deep panel recession)
    g.setColour(juce::Colours::black.withAlpha(0.98f));
    for (float i = 5.0f; i <= 7.0f; i += 0.5f)
      g.drawRoundedRectangle(r.reduced(i), curRadius, 1.0f);

    // 8. Outer Drop Shadow (Enhanced weight)
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(r.expanded(1.2f).translated(2.0f, 2.0f), curRadius,
                           2.0f);

    // 8. Bottom Rim Reflection (Light from the ground)
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(r.reduced(0.5f).translated(1.8f, 1.8f), curRadius,
                           0.4f);
  };
  // Sections and Titles below
  // --- 3D Screwed-On Badge ---
  {
    auto badge =
        L.topBar.reduced(2.0f); // Match the exact thickness of Input Meter
    float cr = 6.0f;            // Corner radius for badge

    g.saveState();
    // Badge Background (Dark Metal)
    juce::Colour b1 = juce::Colour(0xff121213); // Dark Steel (minimal darker)
    juce::Colour b2 = juce::Colour(0xff040404);
    juce::ColourGradient bg(b1, badge.getX(), badge.getY(), b2,
                            badge.getRight(), badge.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(badge, cr);

    // Badge Border (Bevel) - Silver/Chrome
    g.setColour(juce::Colour(0xffd0d0d0)); // Brighter Chrome
    g.drawRoundedRectangle(badge.expanded(0.5f), cr, 1.8f);
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(badge.reduced(1.0f), cr, 0.8f);

    // GLASS REFLECTION LAYER
    juce::Path glass;
    glass.addRectangle(badge.getX(), badge.getY(), badge.getWidth(),
                       badge.getHeight() * 0.45f);
    juce::ColourGradient gg(juce::Colours::white.withAlpha(0.18f), badge.getX(),
                            badge.getY(), juce::Colours::transparentWhite,
                            badge.getX(),
                            badge.getY() + badge.getHeight() * 0.5f, false);
    g.setGradientFill(gg);
    g.fillPath(glass);

    // Subtle diagonal sweep
    juce::Path sweep;
    sweep.addRectangle(badge.getX(), badge.getY() + 10, badge.getWidth(), 2);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillPath(sweep);

    // 4 Small Screws for the Badge (Silver)
    auto drawSmallScrew = [&](float x, float y) {
      float s = 10.0f;
      juce::Rectangle<float> sr(x - s / 2, y - s / 2, s, s);
      g.setColour(juce::Colours::black.withAlpha(0.8f));
      g.fillEllipse(sr.translated(1, 1));

      juce::ColourGradient sg(juce::Colour(0xffe0e0e0), sr.getX(), sr.getY(),
                              juce::Colour(0xff606060), sr.getRight(),
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
        juce::FontOptions("CartoonVibes", 38.0f, juce::Font::plain));
    juce::Font subFont(
        juce::FontOptions("CartoonVibes", 30.0f, juce::Font::plain));
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
        juce::Colour c = juce::Colour(0xffcfc5b0); // Default Bright White

        if (i < 5)
          c = currentPalette.accent; // FUNKY -> Ampeg Blue
        else if (i >= 6 && i <= 10)
          c = currentPalette.labelText; // MOOSE -> Silver/White

        // 1. Glow/Bloom behind - Reduced for BASS STRATEGY
        float glowAlpha = (i < 12) ? 0.15f : 0.05f;
        g.setColour(c.withAlpha(glowAlpha));
        g.drawText(charStr, (int)curX, (int)(badge.getY()), (int)charW + 10,
                   (int)badge.getHeight(), juce::Justification::centred, false);

        // Randomly fade some letters for worn look
        juce::Random textRng(fullText.hashCode() + i);
        float wearAlpha = 0.5f + textRng.nextFloat() * 0.5f;
        c = c.withAlpha(wearAlpha);

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
      juce::Colour(0xff0a0a0c); // Deep neutral charcoal (cool)

  // Subtle color temperature tints (studio lighting feel)
  const auto warmTint =
      juce::Colour::fromRGB(200, 205, 215); // Neutral Steel for AMP
  const auto coolTint =
      juce::Colour::fromRGB(180, 200, 220); // Cool blue for COMP
  const auto greenTint = juce::Colour::fromRGB(190, 200, 190); // Neutral for FX

  // Meter / Master (Raised)
  drawFrame(L.meter, -1.0f, -1.0f, darkFrameCol, false, 0.6f);

  // AMP section frame
  {
    const bool isOn = ampOnToggle.getToggleState();
    const float darken = isOn ? 1.0f : 0.65f;
    const juce::Colour tint =
        isOn ? juce::Colours::transparentBlack : juce::Colours::black;
    drawFrame(L.amp, -1.0f, -1.0f, juce::Colours::transparentBlack, true, -0.4f,
              darken, tint, 0);
    // section label moved to left side elsewhere; avoid duplicate in centre
    // drawLabel(g, L.amp.withHeight(32.0f).translated(0, 4),
    // "AMPLIFIER", 20.0f);
  }

  // COMP section frame
  {
    const bool isOn = compOn.getToggleState();
    const float darken = isOn ? 0.92f : 0.65f;
    const juce::Colour tint = isOn ? coolTint : juce::Colours::black;
    drawFrame(L.comp, -1.0f, -1.0f, juce::Colours::transparentBlack, true, 0.0f,
              darken, tint, 0);
    // drawLabel(g, L.comp.withHeight(32.0f).translated(0, 4), "COMPRESSOR",
    //           20.0f);
  }

  // FX slots
  for (int i = 0; i < 4; ++i) {
    const juce::ToggleButton *toggles[] = {&octOn, &envOn, &phaserOn,
                                           &chorusOn};
    const bool isActive = toggles[i]->getToggleState();
    const float darken = isActive ? 0.85f : 0.60f;
    const juce::Colour tint =
        isActive ? juce::Colours::transparentBlack : juce::Colours::black;

    drawFrame(L.fxSlots[i], -1.0f, -1.0f, juce::Colours::transparentBlack, true,
              -0.2f, darken, tint, 1);
    // central fx slot label removed to avoid duplication with left-hand heading
    // drawLabel(g, L.fxSlots[i].withHeight(28.0f), L.fxNames[i], 16.0f,
    //           juce::Justification::centred);
  }

  // Master / Output Meter (Polished silver/steel Frontplate)
  {
    const bool isOn = masterOn.getToggleState();
    const float darken = isOn ? 1.0f : 0.65f;
    const juce::Colour tint =
        isOn ? juce::Colours::transparentBlack : juce::Colours::black;
    drawFrame(L.master, -1.0f, -1.0f, juce::Colours::transparentBlack, true,
              0.4f, darken, tint, 2);
  }

  drawFrame(L.outVuArea, -1.0f, -1.0f, darkFrameCol, true, 0.4f, 1.1f,
            juce::Colours::transparentBlack, 2);
  drawFrame(L.elchArea, 0.0f, 1.5f, darkFrameCol, true, -0.2f);

  // Titles disabled as requested (but header space reserved for badges)
  g.setColour(juce::Colour(0xffcfc5b0));

  auto titleAt = [&](juce::Rectangle<float> r, const juce::String &t,
                     juce::Justification just = juce::Justification::left) {
    auto header = r.reduced(14.0f, 5.0f).removeFromTop(26.0f);
    drawLabel(g, header, t, 17.0f, just);
  };

  titleAt(L.amp.reduced(84.0f, 0.0f), "AMP", juce::Justification::centred);
  titleAt(L.comp.reduced(84.0f, 0.0f), "COMPRESSOR",
          juce::Justification::centred);

  // FX Module Titles: Moved to bottom-left corner to avoid knob overlap
  for (int i = 0; i < 4; ++i) {
    auto slot = L.fxSlots[(size_t)i];
    auto titleArea = slot.reduced(14.0f, 6.0f).removeFromBottom(24.0f);
    // Draw in the corner with a nice 3D effect
    drawLabel(g, titleArea, L.fxNames[i], 16.0f, juce::Justification::left);
  }

  // MASTER OUT: Perfectly centered between Auto switch and Mono Maker knob
  {
    auto header = L.master.reduced(14.0f, 5.0f).removeFromTop(26.0f);
    float autoSwitchEnd = 84.0f;
    float monoMakerStart = monoMakerKnob.getX() - L.master.getX();
    auto titleArea = header.withLeft(header.getX() + autoSwitchEnd)
                         .withRight(L.master.getX() + monoMakerStart);
    drawLabel(g, titleArea, "MASTER OUT", 17.0f, juce::Justification::centred);
  }

  // --- TOP ROW / STATS ---
  // ELCH Title - Larger, thicker, more 3D
  drawLabel(g,
            L.elchArea.reduced(14.0f, 5.0f)
                .removeFromTop(32.0f)
                .translated(75.0f, 8.0f),
            "ELCH / VISUAL / RMS", 20.0f, juce::Justification::left, true);

  // Knob labels
  auto labelUnder = [&](juce::Component &c, const juce::String &t,
                        float size = 16.5f,
                        juce::Colour col = juce::Colour(0xffcfc5b0)) {
    auto b = c.getBounds().toFloat();
    g.setColour(col);
    // Use the 3D drawLabel instead of raw Graphics calls for consistency
    drawLabel(
        g,
        {b.getX() - 12.0f, b.getBottom() + 10.0f, b.getWidth() + 24.0f, 22.0f},
        t, size, juce::Justification::centredTop);
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

    auto cxLC = bLC.getCentreX();
    auto cxT = bT.getCentreX();
    auto cxS = bS.getCentreX();

    // Unified Labeling for Amp Toggles - BELOW the switches (Centered &
    // slightly larger)
    drawLabel(g, {cxLC - 60.0f, bLC.getBottom() + 1.0f, 120.0f, 16.0f},
              "LOW CUT", 12.0f, juce::Justification::centredTop);
    drawLabel(g, {cxLC - 60.0f, bLC.getBottom() + 15.0f, 120.0f, 14.0f},
              "40 Hz", 11.0f, juce::Justification::centredTop);

    drawLabel(g, {cxT - 60.0f, bT.getBottom() + 1.0f, 120.0f, 16.0f}, "TUBE",
              12.0f, juce::Justification::centredTop);

    drawLabel(g, {cxS - 60.0f, bS.getBottom() + 1.0f, 120.0f, 16.0f}, "SLAP",
              12.0f, juce::Justification::centredTop);
    drawLabel(g, {cxS - 60.0f, bS.getBottom() + 15.0f, 120.0f, 14.0f},
              "+5 dB @ 8k", 10.5f, juce::Justification::centredTop);
  }

  // MASTER & CAB (Add shadow for the large knob to feel less isolated)
  {
    auto b = outKnob.getBounds().toFloat().reduced(6.0f);
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillEllipse(b.translated(0, 6.0f)); // Drop shadow
  }
  // Removed "VOLUME" label under outKnob as requested

  // COMP
  labelUnder(compInKnob, "INPUT", 15.0f);
  labelUnder(compThreshKnob, "THRESH", 15.0f);
  labelUnder(compMakeKnob, "MAKEUP", 15.0f);
  labelUnder(compAtkKnob, "ATTACK", 15.0f);
  labelUnder(compRelKnob, "RELEASE", 15.0f);

  g.setColour(juce::Colour(0xffcfc5b0));
  {
    auto b = ratioBox.getBounds().toFloat();
    // draw label *above* the combo box instead of below
    drawLabel(g, {b.getX(), b.getY() - 20.0f, b.getWidth(), 18.0f}, "RATIO",
              12.0f, juce::Justification::centredBottom);
  }

  // Helper for ON/OFF labels above toggles
  auto labelToggle = [&](juce::ToggleButton &t) {
    auto b = t.getBounds().toFloat();
    // Move tiefer: from -16 to -10, and wider box for perfect centering
    drawLabel(g,
              {b.getX() - 20.0f, b.getY() - 10.0f, b.getWidth() + 40.0f, 16.0f},
              "ON/OFF", 11.0f, juce::Justification::centredBottom);
  };

  labelToggle(ampOnToggle);
  labelToggle(compOn);
  labelToggle(masterOn);

  // Manual draw to move it higher
  {
    auto b = mixKnob.getBounds().toFloat();
    g.setColour(currentPalette.labelText.withAlpha(0.7f));
    drawLabel(
        g,
        {b.getX() - 12.0f, b.getBottom() - 1.0f, b.getWidth() + 24.0f, 20.0f},
        "DRY/WET", 15.0f, juce::Justification::centredTop);
  }

  // MONO MAKER label
  {
    auto bK = monoMakerKnob.getBounds().toFloat();
    g.setColour(currentPalette.labelText.withAlpha(0.7f));

    drawLabel(g,
              {bK.getX() - 20.0f, bK.getBottom() - 1.0f, bK.getWidth() + 40.0f,
               20.0f},
              "MONO MAKER", 14.5f, juce::Justification::centredTop);
  }

  // Labels for sub-toggles (Modern/Parallel/Auto)
  auto labelSubToggle = [&](juce::ToggleButton &t, const juce::String &txt) {
    auto b = t.getBounds().toFloat();
    drawLabel(
        g,
        {b.getX() - 15.0f, b.getBottom() + 1.0f, b.getWidth() + 30.0f, 16.0f},
        txt, 12.0f, juce::Justification::centredTop);
  };

  // POSITIONED ABOVE for Master area (to clear screws/corner elements)
  auto labelSubToggleMaster = [&](juce::ToggleButton &t,
                                  const juce::String &txt) {
    auto b = t.getBounds().toFloat();
    drawLabel(g,
              {b.getX() - 15.0f, b.getY() - 10.0f, b.getWidth() + 30.0f, 16.0f},
              txt, 12.0f, juce::Justification::centredBottom);
  };

  // POSITIONED ABOVE for Master area (to clear screws/corner elements)
  auto labelSubMaster = [&](juce::Slider &s, const juce::String &txt) {
    auto b = s.getBounds().toFloat();
    g.setColour(juce::Colour(0xffcfc5b0).withAlpha(0.7f));
    drawLabel(g,
              {b.getX() - 15.0f, b.getY() - 15.0f, b.getWidth() + 30.0f, 16.0f},
              txt, 11.0f, juce::Justification::centredBottom);
  };

  labelSubToggleMaster(ampAutoGainToggle, "AUTO");
  labelSubToggle(autoGateToggle, "GATE");
  labelSubToggleMaster(compAutoMakeupToggle, "AUTO");
  labelSubToggleMaster(autoGainToggle, "AUTO");
  labelSubToggleMaster(fxParallelToggle, "PARALLEL");
  labelSubToggle(monoMakerToggle, "ON");

  // --- CAB Button Recessed Frame (Window Style) ---
  {
    auto b = cabButton.getBounds().toFloat();
    // Industrial recessed slot (The Frame) - Deeper recessed effect
    drawFrame(b.expanded(2.0f), 4.0f, 1.8f, juce::Colour(0xff080807), true,
              -0.6f);

    // Glass/Recessed Glow inside the window
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(b, 3.0f);

    // Inner Shadow line
    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.drawRoundedRectangle(b, 3.0f, 1.2f);

    // Cabinet Label BELOW the slot - Position adjusted to align with knobs
    // g.setColour(currentPalette.labelText.withAlpha(0.7f));
    // drawLabel(g, {b.getX(), b.getBottom() + 7.0f, b.getWidth(), 20.0f},
    //           "CABINET", 15.0f, juce::Justification::centredTop);
  }

  // IR MIX Label - Perfectly aligned BELOW the knob
  {
    auto b = irMixKnob.getBounds().toFloat();
    g.setColour(currentPalette.labelText.withAlpha(0.7f));
    drawLabel(
        g,
        {b.getX() - 20.0f, b.getBottom() - 1.0f, b.getWidth() + 40.0f, 20.0f},
        "IR MIX", 14.5f, juce::Justification::centredTop);
  }

  // FX parameter labels
  g.setColour(sub);
  labelUnder(oct1Knob, "-1 OCT", 15.0f, sub);
  labelUnder(oct2Knob, "+1 OCT", 15.0f, sub);
  labelUnder(octMixKnob, "MIX", 15.0f, sub);

  labelToggle(octOn);
  labelToggle(envOn);
  labelToggle(phaserOn);
  labelToggle(chorusOn);

  labelUnder(envAtkKnob, "ATTACK", 15.0f, sub);
  labelUnder(envDecKnob, "DECAY", 15.0f, sub);
  labelUnder(envRangeKnob, "RANGE", 15.0f, sub);

  labelUnder(phRateKnob, "RATE", 15.0f, sub);
  labelUnder(phColKnob, "COLOUR", 15.0f, sub);
  labelUnder(phMixKnob, "MIX", 15.0f, sub);

  labelUnder(chRateKnob, "RATE", 15.0f, sub);
  labelUnder(chDepthKnob, "DEPTH", 15.0f, sub);
  labelUnder(chMixKnob, "MIX", 15.0f, sub);

  // MASTER - Overlay
  // If a pre-rendered skin overlay exists, draw it; otherwise nothing.
  if (!cachedSkinOverlay.isNull()) {
    g.drawImage(cachedSkinOverlay, 0, 0, (float)designW, (float)designH, 0, 0,
                cachedSkinOverlay.getWidth(), cachedSkinOverlay.getHeight(),
                false);
  }

  // --- Module Mounting Screws (Rusty & Abused) ---
  auto drawModScrew = [&](juce::Point<float> p) {
    float s = 13.0f;
    juce::Rectangle<float> sr(p.x - s / 2, p.y - s / 2, s, s);

    // Hole shadow / deep dirt
    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.fillEllipse(sr.translated(0.0f, 1.0f).expanded(2.0f)); // Thick grime ring

    // Screw Head (Dark metal, clean black)
    juce::ColourGradient sg(juce::Colour(0xff252528), sr.getX(), sr.getY(),
                            juce::Colours::black, sr.getRight(), sr.getBottom(),
                            false);
    g.setGradientFill(sg);
    g.fillEllipse(sr.reduced(0.5f));

    // Stronger Inner Shadow
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(sr.reduced(1.2f), 1.2f);

    // Cross Slot (Regular)
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(
        p.x * 0.05f + p.y * 0.1f, sr.getCentreX(),
        sr.getCentreY())); // Random rotation

    float slotW = 3.5f;
    float slotH = 5.0f;
    g.setColour(juce::Colours::black);
    g.fillRect(sr.reduced(slotW, slotH));
    g.fillRect(sr.reduced(slotH, slotW));
    g.restoreState();

    // Tiny rim highlight (Muted because of dirt)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
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
  addScrews(L.elchArea, modInset);

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
  // 1. Draw Cached Static Background
  if (cachedContentBackground.isValid() && !cachedContentBackground.isNull()) {
    g.drawImageAt(cachedContentBackground, 0, 0);
  } else {
    updateStaticBackground();
    if (cachedContentBackground.isValid())
      g.drawImageAt(cachedContentBackground, 0, 0);
  }

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

  // Module numbering disabled as requested
  /*
  drawStatusBadge(L.meter, 1, true); // Input always "on"
  drawStatusBadge(L.amp, 2, ampOnToggle.getToggleState());
  drawStatusBadge(L.comp, 3, compOn.getToggleState());
  drawStatusBadge(L.fxSlots[0], 4, octOn.getToggleState());
  drawStatusBadge(L.fxSlots[1], 5, envOn.getToggleState());
  drawStatusBadge(L.fxSlots[2], 6, phaserOn.getToggleState());
  drawStatusBadge(L.fxSlots[3], 7, chorusOn.getToggleState());
  drawStatusBadge(L.master, 8, masterOn.getToggleState());
  */

  // --- MASTER OUT KNOB GLOW (Output Level Feedback) ---
  {
    auto bOut = outKnob.getBounds().toFloat();
    if (masterOn.getToggleState()) {
      if (cachedMasterGlow.isNull())
        updateGlowCaches();

      // Dynamic glow based on output level
      float outLevel = outVu.level; // Use actual output meter level
      float pulse = 0.5f + 0.5f * std::sin(time * 3.5f);
      float baseAlpha =
          0.45f + 0.45f * outLevel + 0.12f * pulse; // Slightly boosted

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
      g.setColour(glowCol.withAlpha(0.75f * baseAlpha)); // Sharper inner ring
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

    // Height: Moved significantly further DOWN to 46.0f to clear centered title
    float ly = headerRect.getY() + 46.0f;
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

  if (area.isEmpty())
    return;

  const float sx = area.getWidth() / (float)designW;
  const float sy = area.getHeight() / (float)designH;
  const float s = std::min(sx, sy);

  content.setTransform(juce::AffineTransform::scale(s));

  const int cw = (int)std::round(designW * s);
  const int ch = (int)std::round(designH * s);

  content.setTopLeftPosition((getWidth() - cw) / 2, (getHeight() - ch) / 2);

  content.setSize(designW, designH);

  // Ensure background is valid for new size/scale
  if (cachedContentBackground.isNull() ||
      cachedContentBackground.getWidth() != designW ||
      cachedContentBackground.getHeight() != designH) {
    updateStaticBackground();
  }

  overlayComp.setBounds(0, 0, designW, designH);
  elch.toFront(false);
}

void FunkyMooseAudioProcessorEditor::drawLabel(
    juce::Graphics &g, juce::Rectangle<float> r, const juce::String &text,
    float size, juce::Justification just, bool isTopRow) const {
  // Snap to pixel grid for crispness
  r = r.withPosition(std::round(r.getX()), std::round(r.getY()))
          .withSize(std::round(r.getWidth()), std::round(r.getHeight()));

  juce::Font font(size, juce::Font::bold);
  font.setHorizontalScale(1.03f); // Subtle tracking for premium feel
  g.setFont(font);

  // --- PREMIUM 3D TEXT EFFECT ---

  // 1. Core Deep Shadow (The depth)
  g.setColour(juce::Colours::black.withAlpha(0.65f));
  g.drawFittedText(text, r.translated(1.5f, 1.8f).toNearestInt(), just, 1,
                   0.9f);

  // 2. Mid Shadow (Softening the transition)
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.drawFittedText(text, r.translated(0.8f, 1.0f).toNearestInt(), just, 1,
                   0.9f);

  // 3. Bottom Catch-Light (Embossed edge)
  g.setColour(juce::Colours::white.withAlpha(isTopRow ? 0.35f : 0.18f));
  g.drawFittedText(text, r.translated(0.0f, 1.0f).toNearestInt(), just, 1,
                   0.9f);

  // 4. Main Text Layer
  // Top row is pure White, others Steel Silver
  g.setColour(isTopRow ? juce::Colours::white : juce::Colour(0xffcfc5b0));
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

  // Pre-calculate common column widths to perfectly align sections vertically
  const float wRight = std::floor(content.getWidth() * 0.28f); // Elch Width
  const float wLeftCol = content.getWidth() - wRight - G;
  const float wAmp = std::floor(wLeftCol * 0.55f); // Amp is 55% of LeftCol

  // 1. Top Row (Input Meter | Title)
  const float hTop = std::floor(content.getHeight() * 0.16f);
  auto topRow = content.removeFromTop(hTop);
  content.removeFromTop(G);

  // Meter is explicitly wAmp to match Output Meter
  L.meter = topRow.removeFromLeft(wAmp);
  topRow.removeFromLeft(G);
  // Title takes the remaining space on the top row
  L.topBar = topRow;

  // 2. Bottom Row (Output Meter | Master Knob)
  const float hBottom = std::floor(content.getHeight() * 0.14f);
  auto bottomRow = content.removeFromBottom(hBottom);
  content.removeFromBottom(G);

  // Bottom row split identically to Top row to match Meter sizes
  L.outVuArea = bottomRow.removeFromLeft(wAmp); // Match Amp width exactly
  bottomRow.removeFromLeft(G);
  L.master = bottomRow; // Rest is Master section

  // 3. Center Row (Left: Controls, Right: Elch)
  // Right Col width ~40%
  // Right Col width ~40%
  // wRight already defined above
  L.elchArea = content.removeFromRight(wRight);
  content.removeFromRight(G);

  auto leftCol = content;

  // Split Left Col into Amp|Comp (Top) and FX (Bottom)
  // Split 50/50 and remove the gap 'G' entirely so they perfectly touch.
  const float hUpper = std::floor(leftCol.getHeight() * 0.50f);
  auto upperRow = leftCol.removeFromTop(hUpper);
  // (Removed leftCol.removeFromTop(G) to seal the gap completely)
  auto lowerRow = leftCol;

  // Upper: Amp (55%) | Comp (45%)
  L.amp = upperRow.removeFromLeft(wAmp); // Use pre-calculated width
  upperRow.removeFromLeft(G);
  L.comp = upperRow;

  // Lower: FX Grid 2x2
  L.fx = lowerRow; // Container for entire FX

  // We removed the title strip space so the FX slots can touch the Amp/Comp
  // directly
  const float hTitle = 0.0f;
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
  float topY = 44.0f; // Adjusted to align with horizontal lines "einrasten"
  float rightX = (float)designW - 140.0f; // Margin

  // Increased sizes for top row (Box longer/wider)
  float boxH = 34.0f; // 28 -> 34

  // Fold Button
  openFolderButton.setBounds((int)(rightX - 60.0f), (int)(topY - 3.0f), 60,
                             (int)boxH);
  rightX -= 65.0f;

  // Save Button
  savePresetButton.setBounds((int)(rightX - 60.0f), (int)(topY - 3.0f), 60,
                             (int)boxH);
  rightX -= 65.0f;

  // Preset Selector
  presetSelector.setBounds((int)(rightX - 250.0f), (int)(topY - 3.0f), 250,
                           (int)boxH);

  // New: Tooltip Toggle (Left of Preset Selector)
  toggleTooltips.setBounds(presetSelector.getX() - 125, (int)(topY - 3.0f), 120,
                           (int)boxH);

  // Stats HUD (Left of Tooltips) - Much wider ("longer") and nudged right
  statsHUD.setBounds(toggleTooltips.getX() - 185, (int)(topY - 3.0f), 175,
                     (int)boxH);

  // Mono Toggle (Left of CPU Stats)
  monoInputButton.setBounds(statsHUD.getX() - 100, (int)(topY - 3.0f), 95,
                            (int)boxH);

  // MIDI Indicator (Left of Mono Toggle)
  midiIndicator.setBounds(monoInputButton.getX() - 36, (int)(topY - 3.0f), 32,
                          (int)boxH);

  // Ensure visibility
  presetSelector.toFront(false);
  savePresetButton.toFront(false);
  openFolderButton.toFront(false);
  toggleTooltips.toFront(false);
  statsHUD.toFront(false);
  midiIndicator.toFront(false); // MUST be in front of the overlayComp
  monoInputButton.toFront(false);

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
      inRect.withSizeKeepingCentre(inRect.getWidth(), 56.0f).toNearestInt();
  inVu.setBounds(inVuBounds);
  tunerOverlay->setBounds(inVuBounds);

  // Tuner Toggle placement: Horizontal and wide, centered under the meter bar
  tunerToggle.setBounds(L.meter.getCentreX() - 65, L.meter.getY() + 85, 130,
                        32);

  // Slimmer Output Meter (Centred Vertically)
  auto outRect = L.outVuArea.reduced(16.0f); // Match Input Meter margin
  outVu.setBounds(
      outRect.withSizeKeepingCentre(outRect.getWidth(), 56.0f).toNearestInt());

  elch.setBounds(L.elchArea.reduced(16.0f).toNearestInt());

  // --- 4. TOGGLES (Small Lamps) ---
  auto placeToggle = [&](juce::ToggleButton &t, juce::Rectangle<float> frame) {
    // Align X so the 54px button inside has exactly 15px margin from the
    // right edge Button Right = R - 15 -> Button X = R - 15 - 54 = R - 69
    // ToggleBox X = Button X - (84-54)/2 = R - 69 - 15 = R - 84
    float x = frame.getRight() - 84.0f;
    t.setBounds((int)x, (int)(frame.getY() + 10.0f), 84, 40);
  };

  auto placeToggleLeft = [&](juce::ToggleButton &t,
                             juce::Rectangle<float> frame) {
    float xPos = frame.getX();
    // Shift Compressor Auto makeup right to clear the GR meter (Reduced to
    // 24px per user request)
    if (&t == &compAutoMakeupToggle)
      xPos += 24.0f;
    t.setBounds((int)xPos, (int)(frame.getY() + 10.0f), 84, 40);
  };

  placeToggle(compOn, L.comp);
  placeToggleLeft(compAutoMakeupToggle, L.comp);

  placeToggle(ampOnToggle, L.amp);
  placeToggleLeft(ampAutoGainToggle, L.amp);

  placeToggle(masterOn, L.master);
  placeToggleLeft(autoGainToggle, L.master);

  // FX toggles per slot
  for (int i = 0; i < 4; ++i) {
    auto frame = L.fxSlots[(size_t)i];

    switch (i) {
    case 0:
      placeToggle(octOn, frame);
      break;
    case 1:
      placeToggle(envOn, frame);
      break;
    case 2:
      placeToggle(phaserOn, frame);
      break;
    case 3:
      placeToggle(chorusOn, frame);
      placeToggleLeft(fxParallelToggle, frame);
      break;
    }
  }

  // --- 5. GRID CONTENT ---
  // ... rest of the standard logic remains unchanged ...
}

void FunkyMooseAudioProcessorEditor::layoutAmp(
    const juce::Rectangle<float> &r) {
  constexpr float kPrimary = 106.0f; // Increased knob size for thicker module
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

  // Main Toggles Row (Gap distribution for symmetry)
  float x0 = row.getX() + k / 2.0f;
  float step = k + G;

  float gap1 = x0 + step * 0.5f;
  float gap2 = x0 + step * 1.5f;
  float gap3 = x0 + step * 2.5f;
  float gap4 = x0 + step * 3.5f; // Where old Auto-Gain was

  lowCutToggle.setBounds((int)(gap1 - swW / 2.0f), (int)swY, (int)swW,
                         (int)swH);
  tubeToggle.setBounds((int)(gap2 - swW / 2.0f), (int)swY, (int)swW, (int)swH);
  slapToggle.setBounds((int)(gap3 - swW / 2.0f), (int)swY, (int)swW, (int)swH);
  autoGateToggle.setBounds((int)(gap4 - swW / 2.0f), (int)swY, (int)swW,
                           (int)swH);
}

void FunkyMooseAudioProcessorEditor::layoutComp(
    const juce::Rectangle<float> &r) {
  constexpr float kSecondary = 76.0f; // Matched to FX knob size (76px)

  // User requested "2 row layout"
  // Row 1: Input | Threshold | Ratio
  // Row 2: Makeup | Attack | Release

  auto area = r;
  // Shift controls more to the left to clear the vertical GR meter AND the
  // ON/OFF toggle on the right
  area.removeFromLeft(52.0f);
  area.removeFromRight(
      12.0f); // Extra breathing room for the right-side toggles

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
    float slotY = area.getY() + row * rowH;

    // Spread the rows significantly to get labels exactly in the middle
    if (row == 0)
      slotY -= 20.0f; // Give room for top labels
    if (row == 1)
      slotY += 26.0f; // Pull down enough so knobs don't overlap upper labels,
                      // but keep bottom labels safe

    // Center in slot
    float cx = slotX + wSlot * 0.5f;
    float cy = slotY + rowH * 0.5f;

    // Shift the right-most column (Punch/Ratio/Release) a bit more left
    if (col == 2)
      cx -= 15.0f;

    // For combo box (Ratio)
    if (&c == &ratioBox) {
      c.setBounds((int)(cx - 48.0f), (int)(cy - 26.0f), 96, 24);
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
    float slotY = area.getY() + 0 * rowH - 24.0f;

    float cx =
        slotX + wSlot * 0.5f - 15.0f; // Matching the shift from placeInSlot
    float cy = slotY + rowH * 0.5f;
    // Position Punch button lower, below the Ratio box
    punchButton.setBounds((int)(cx - 48.0f), (int)(cy + 8.0f), 96, 28);
  }

  // Row 2
  placeInSlot(compMakeKnob, 1, 0, k);
  placeInSlot(compAtkKnob, 1, 1, k);
  placeInSlot(compRelKnob, 1, 2, k);

  // GR Meter: Vertical on the Left (Maximum Analog Height)
  {
    const auto &L = getLayout();
    float grW = 32.0f;
    float grH = L.comp.getHeight() - 90.0f; // Extended 10px higher
    float grX = L.comp.getX() + 14.0f;
    float grY = L.comp.getY() + 50.0f; // Pulled 5px higher
    compGr.setBounds((int)grX, (int)grY, (int)grW, (int)grH);
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

  constexpr float k = 76.0f;            // FX knob diameter (smaller to fit)
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
    // Shifted UP further to account for labels so they don't clip the bottom
    // frame
    float y = area.getCentreY() - k / 2.0f - 30.0f;

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
  c.y -= 15.0f; // Shift UP safely: -15 preserves room for title

  // Master Knob (Big)
  outKnob.setBounds(
      juce::Rectangle<float>(c.x - 58.0f, c.y - 65.0f, 116.0f, 116.0f)
          .toNearestInt());

  // Mix (Left of Master)
  mixKnob.setBounds(
      juce::Rectangle<float>(c.x - 145.0f, c.y - 25.0f, 50.0f, 50.0f)
          .toNearestInt());

  // Mono Maker Toggle (Between Mix and Mono Maker)
  monoMakerToggle.setBounds(
      juce::Rectangle<float>(c.x - 207.0f, c.y - 7.0f, 24.0f, 24.0f)
          .toNearestInt());

  // Mono Maker (Further Left)
  monoMakerKnob.setBounds(
      juce::Rectangle<float>(c.x - 295.0f, c.y - 25.0f, 50.0f, 50.0f)
          .toNearestInt());

  // IR Mix (Right of Master - Symmetrical to Mix Knob)
  irMixKnob.setBounds(
      juce::Rectangle<float>(c.x + 95.0f, c.y - 25.0f, 50.0f, 50.0f)
          .toNearestInt());

  // Cab (Rightmost - Symmetrical to Mono Maker bounds)
  cabButton.setBounds(
      juce::Rectangle<float>(c.x + 175.0f, c.y - 20.0f, 120.0f, 40.0f)
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

void FunkyMooseAudioProcessorEditor::openIrChooser() {
  auto startDir =
      lastIrDirectory.exists()
          ? lastIrDirectory
          : juce::File::getSpecialLocation(juce::File::userHomeDirectory);

  irChooser = std::make_unique<juce::FileChooser>(
      "Select a Custom IR (WAV/AIF)", startDir, "*.wav;*.aiff;*.aif");

  auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                            juce::FileBrowserComponent::canSelectFiles;

  irChooser->launchAsync(
      folderChooserFlags, [this](const juce::FileChooser &fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
          lastIrDirectory = file.getParentDirectory();
          // Update the DSP
          processor.dspChain.getCabSim().loadCustomIr(file.getFullPathName());

          // Set APVTS parameter to 3 (CUSTOM IR) -> Normalized 1.0f
          auto *p = processor.apvts.getParameter("cabType");
          p->beginChangeGesture();
          p->setValueNotifyingHost(1.0f);
          p->endChangeGesture();
        }
      });
}

void FunkyMooseAudioProcessorEditor::timerCallback() {
  inVu.setLevel(processor.getInRms());
  outVu.setLevel(processor.getOutRms());

  // Update Compressor GR Meter
  compGr.setLevel(juce::Decibels::decibelsToGain(
      processor.dspChain.getCompressor().getGainReductionDb()));

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
    juce::String txt = "CAB: OFF";
    if (idx == 1)
      txt = "CAB: 4x10";
    else if (idx == 2)
      txt = "CAB: 1x15";
    else if (idx == 3)
      txt = "CAB: USER IR";

    if (cabButton.getButtonText() != txt)
      cabButton.setButtonText(txt);
  }

  // --- Visual Dimming for all modules ---
  {
    auto setAlphaForModule = [](bool isOn,
                                const std::vector<juce::Component *> &comps) {
      const float targetAlpha = isOn ? 1.0f : 0.40f;
      for (auto *c : comps) {
        if (std::abs(c->getAlpha() - targetAlpha) > 0.01f)
          c->setAlpha(targetAlpha);
      }
    };

    // Amp
    setAlphaForModule(ampOnToggle.getToggleState(),
                      {&gainKnob, &bassKnob, &midKnob, &trebleKnob, &volumeKnob,
                       &slapToggle, &tubeToggle, &lowCutToggle,
                       &ampAutoGainToggle});

    // Comp
    setAlphaForModule(compOn.getToggleState(),
                      {&compInKnob, &compThreshKnob, &compMakeKnob,
                       &compAtkKnob, &compRelKnob, &ratioBox,
                       &compAutoMakeupToggle, &punchButton});

    // FX Slot 0 (Octaver)
    setAlphaForModule(octOn.getToggleState(),
                      {&oct1Knob, &oct2Knob, &octMixKnob});

    // FX Slot 1 (Envelope)
    setAlphaForModule(envOn.getToggleState(),
                      {&envAtkKnob, &envDecKnob, &envRangeKnob});

    // FX Slot 2 (Phaser)
    setAlphaForModule(phaserOn.getToggleState(),
                      {&phRateKnob, &phColKnob, &phMixKnob});

    // FX Slot 3 (Chorus)
    setAlphaForModule(chorusOn.getToggleState(),
                      {&chRateKnob, &chDepthKnob, &chMixKnob});

    // Master
    setAlphaForModule(masterOn.getToggleState(),
                      {&outKnob, &mixKnob, &monoMakerKnob, &monoMakerToggle,
                       &autoGainToggle});
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
  // Stats Update (CPU only)
  statsHUD.update(processor.getCPUUsage());

  // MIDI Activity Indicator update (polled from processor)
  float pMidi = processor.getMidiActivity();
  if (pMidi > 0.001f) {
    midiActivityIndicatorLevel = 1.0f;  // Reset peak
    processor.midiActivity.store(0.0f); // Consume the event
  } else {
    midiActivityIndicatorLevel *= 0.88f; // Smooth decay
  }
  midiIndicator.setLevel(midiActivityIndicatorLevel);

  // Tuner Check: Ensure text is white
  tunerToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);

  // repaint only the content canvas (keeps it snappy)
  content.repaint();
}

void FunkyMooseAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}
