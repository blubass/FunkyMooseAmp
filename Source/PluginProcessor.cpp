#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FunkyMooseAudioProcessor::FunkyMooseAudioProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParams()) {}

juce::AudioProcessorEditor *FunkyMooseAudioProcessor::createEditor() {
  return new FunkyMooseAudioProcessorEditor(*this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
FunkyMooseAudioProcessor::createParams() {
  using APF = juce::AudioParameterFloat;
  using APB = juce::AudioParameterBool;
  using APC = juce::AudioParameterChoice;

  std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

  // GLOBAL
  p.push_back(std::make_unique<APB>("bypass", "Hard Bypass", false));
  p.push_back(std::make_unique<APB>("autoGate", "Auto Gate", true));
  p.push_back(std::make_unique<APB>("tunerOn", "Tuner", false));

  // AMP
  p.push_back(std::make_unique<APB>("ampOn", "Amp On", true));
  p.push_back(std::make_unique<APF>(
      "ampGain", "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), -6.0f));
  p.push_back(std::make_unique<APF>(
      "ampBass", "Bass", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
  p.push_back(std::make_unique<APF>(
      "ampMid", "Mid", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
  p.push_back(std::make_unique<APF>(
      "ampTreble", "Treble", juce::NormalisableRange<float>(-12.0f, 12.0f),
      0.0f));
  p.push_back(std::make_unique<APF>(
      "ampVolume", "Volume", juce::NormalisableRange<float>(-24.0f, 6.0f),
      -1.0f));
  p.push_back(std::make_unique<APB>("slap", "Slap 8k", false));
  p.push_back(std::make_unique<APB>("tubeOn", "Tube Saturation", false));
  p.push_back(std::make_unique<APB>("lowCutOn", "Low Cut 40Hz", false));
  p.push_back(std::make_unique<APB>("ampAutoGain", "Amp Auto Gain", false));

  // COMPRESSOR
  p.push_back(std::make_unique<APB>("compOn", "Comp On", false));
  p.push_back(std::make_unique<APF>(
      "compInput", "Comp Input", juce::NormalisableRange<float>(-24.0f, 24.0f),
      0.0f));
  p.push_back(std::make_unique<APF>(
      "compThresh", "Comp Thresh", juce::NormalisableRange<float>(-60.0f, 0.0f),
      -18.0f));
  p.push_back(std::make_unique<APF>("compMakeup", "Comp Makeup",
                                    juce::NormalisableRange<float>(0.0f, 24.0f),
                                    2.0f));
  p.push_back(std::make_unique<APC>(
      "compRatio", "Ratio", juce::StringArray{"4:1", "8:1", "12:1", "20:1"},
      0));
  p.push_back(std::make_unique<APF>("compAttack", "Comp Attack",
                                    juce::NormalisableRange<float>(1.0f, 50.0f),
                                    10.0f));
  p.push_back(std::make_unique<APF>(
      "compRelease", "Comp Release",
      juce::NormalisableRange<float>(50.0f, 500.0f), 120.0f));
  p.push_back(std::make_unique<APB>("punch", "Punch", false));
  p.push_back(
      std::make_unique<APB>("compAutoMakeup", "Comp Auto Makeup", false));

  // FX (1x4 bottom)
  p.push_back(std::make_unique<APB>("octOn", "Octaver On", false));
  p.push_back(std::make_unique<APF>(
      "oct1", "Octave 1", juce::NormalisableRange<float>(0.0f, 100.0f), 40.0f));
  p.push_back(std::make_unique<APF>(
      "oct2", "Octave 2", juce::NormalisableRange<float>(0.0f, 100.0f), 40.0f));
  p.push_back(std::make_unique<APF>(
      "octMix", "Oct Mix", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));
  p.push_back(std::make_unique<APB>("octModern", "Modern Mode", false));

  p.push_back(std::make_unique<APB>("envOn", "Envelope On", false));
  p.push_back(std::make_unique<APF>("envAttack", "Attack",
                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                    0.25f));
  p.push_back(std::make_unique<APF>(
      "envDecay", "Decay", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
  p.push_back(std::make_unique<APF>(
      "envRange", "Range", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));

  p.push_back(std::make_unique<APB>("phaserOn", "Phaser On", false));
  p.push_back(std::make_unique<APF>(
      "phRate", "Rate", juce::NormalisableRange<float>(0.0f, 1.0f), 0.45f));
  p.push_back(std::make_unique<APF>(
      "phColour", "Colour", juce::NormalisableRange<float>(0.0f, 1.0f), 0.55f));
  p.push_back(std::make_unique<APF>(
      "phMix", "Ph Mix", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));

  p.push_back(std::make_unique<APB>("chorusOn", "Chorus On", false));
  p.push_back(std::make_unique<APF>(
      "chRate", "Rate", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
  p.push_back(std::make_unique<APF>(
      "chDepth", "Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.55f));
  p.push_back(std::make_unique<APF>(
      "chMix", "Ch Mix", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));
  p.push_back(std::make_unique<APB>("fxParallel", "Parallel FX", false));

  // MASTER
  p.push_back(std::make_unique<APF>(
      "masterOut", "Output", juce::NormalisableRange<float>(-60.0f, 6.0f),
      -1.0f));
  p.push_back(std::make_unique<APC>(
      "cabType", "Cabinet",
      juce::StringArray{"OFF", "4x10", "1x15", "CUSTOM IR"}, 1));
  p.push_back(std::make_unique<APF>(
      "irMix", "IR Mix", juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f));
  p.push_back(std::make_unique<APF>(
      "masterMix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f),
      100.0f));
  p.push_back(std::make_unique<APC>(
      "skin", "Skin",
      juce::StringArray{"Classic", "Midnight", "Vintage", "Electric", "Used Up",
                        "Bloody", "Orange", "Ampeg", "Toxic"},
      0));

  p.push_back(std::make_unique<APB>("autoGain", "Auto Gain", false));
  p.push_back(std::make_unique<APF>(
      "monoMaker", "Mono Maker", juce::NormalisableRange<float>(20.0f, 400.0f),
      20.0f));
  p.push_back(std::make_unique<APB>("monoMakerOn", "Mono Maker On/Off", true));

  return {p.begin(), p.end()};
}

//==============================================================================
void FunkyMooseAudioProcessor::prepareToPlay(double sampleRate,
                                             int samplesPerBlock) {
  mState.spec.sampleRate = sampleRate;
  mState.spec.maximumBlockSize = (juce::uint32)juce::jmax(1, samplesPerBlock);
  mState.spec.numChannels =
      (juce::uint32)juce::jmax(1, getTotalNumOutputChannels());

  dspChain.prepare(mState.spec);

  dryBuffer.setSize(mState.spec.numChannels, mState.spec.maximumBlockSize);

  // Report Latency (Oversampling)
  setLatencySamples((int)std::round(dspChain.getLatency()));

  // LowCut Initialization (40Hz HPF)
  *dspChain.getLowCut().state =
      *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 40.0f);

  mState.prepared = true;

  tunerFifo.prepare(1, 16384);
  tunerMuteSmoothed.reset(sampleRate, 0.05); // 50ms fade
  tunerMuteSmoothed.setCurrentAndTargetValue(1.0f);
}

bool FunkyMooseAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  const auto in = layouts.getMainInputChannelSet();
  const auto out = layouts.getMainOutputChannelSet();

  if (in.isDisabled() || out.isDisabled())
    return false;

  // Standalone can negotiate mono input -> stereo output
  if (in == juce::AudioChannelSet::mono() &&
      out == juce::AudioChannelSet::stereo())
    return true;

  if (out == juce::AudioChannelSet::mono() ||
      out == juce::AudioChannelSet::stereo())
    return in == out;

  return false;
}

void FunkyMooseAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                            juce::MidiBuffer &) {
  juce::ScopedNoDenormals noDenormals;

  const bool tunerOn = (apvts.getRawParameterValue("tunerOn")->load() > 0.5f);
  tunerIsOn.store(tunerOn, std::memory_order_relaxed);

  const int totalNumInputChannels = getTotalNumInputChannels();
  const int totalNumOutputChannels = getTotalNumOutputChannels();

  // Clear unused channels
  // IMPORTANT: In JUCE Standalone, the device can temporarily report 0 input
  // channels (or no inputs enabled). If we clear using 0, we'd wipe the whole
  // buffer and get silence.
  if (totalNumInputChannels > 0)
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
      buffer.clear(ch, 0, buffer.getNumSamples());

  // Mono -> Stereo (e.g. in DAWs with 1-In/2-Out configuration)
  if (totalNumInputChannels == 1 && buffer.getNumChannels() >= 2) {
    buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
  }
  // Standalone: Audio Interfaces always provide stereo (In 1=L, In 2=R).
  // A bass is mono. To prevent it only playing on the left ear, we sum L+R.
  else if (wrapperType == juce::AudioProcessor::wrapperType_Standalone &&
           buffer.getNumChannels() >= 2) {
    auto *L = buffer.getWritePointer(0);
    auto *R = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      float monoSum =
          L[i] +
          R[i]; // If Bass is only on L, R is 0. Sum gives perfect unity center.
      L[i] = monoSum;
      R[i] = monoSum;
    }
  }

  // Tuner Tap (Pre-Gate/Amp)
  if (tunerOn) {
    const int n = buffer.getNumSamples();
    tunerScratchMono.setSize(1, n, false, false, true);
    auto *m = tunerScratchMono.getWritePointer(0);
    auto *L = buffer.getReadPointer(0);
    auto *R =
        (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
      m[i] = 0.5f * (L[i] + (R ? R[i] : L[i]));

    tunerFifo.push(m, n);
  }

  if (!mState.prepared)
    prepareToPlay(getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                  buffer.getNumSamples());

  // HARD DSP BYPASS
  // If active, we skip all processing. Input is already in 'buffer' (upmixed if
  // needed).
  if (apvts.getRawParameterValue("bypass")->load() > 0.5f) {
    // Just pass through. We might want to fade, but "Hard" usually means
    // immediate. We already cleared unused channels. Input is passed to output.
    return;
  }

  // ===== UPDATE PARAMETERS =====

  // 1. Octaver / Env
  {
    auto &oct = dspChain.getOctEnv();
    oct.setOctaveOn(apvts.getRawParameterValue("octOn")->load() > 0.5f);
    oct.setOctave1(apvts.getRawParameterValue("oct1")->load() / 100.0f);
    oct.setOctave2(apvts.getRawParameterValue("oct2")->load() / 100.0f);
    oct.setOctaveMix(apvts.getRawParameterValue("octMix")->load() / 100.0f);
    oct.setModernMode(apvts.getRawParameterValue("octModern")->load() > 0.5f);

    oct.setEnvelopeOn(apvts.getRawParameterValue("envOn")->load() > 0.5f);
    oct.setEnvAttack(apvts.getRawParameterValue("envAttack")->load());
    oct.setEnvDecay(apvts.getRawParameterValue("envDecay")->load());
    oct.setEnvRange(apvts.getRawParameterValue("envRange")->load() / 100.0f);
  }

  // 1.5 Smart Gate
  {
    auto &gate = dspChain.getSmartGate();
    gate.setEnabled(apvts.getRawParameterValue("autoGate")->load() > 0.5f);
  }

  // 2. Input Gain (Drive) + Comp Input
  {
    float compInDb = apvts.getRawParameterValue("compInput")->load();
    float ampGainDb = apvts.getRawParameterValue("ampGain")->load();
    dspChain.getInputGain().setGainDecibels(compInDb + ampGainDb);
  }

  // 3. Amp / Tone
  {
    auto &amp = dspChain.getAmpTone();
    bool ampOn = apvts.getRawParameterValue("ampOn")->load() > 0.5f;
    // Reset if enabled state changes
    if (ampOn != lastAmpOn) {
      amp.reset();
      lastAmpOn = ampOn;
    }
    amp.setAmpOn(ampOn);
    amp.setTubeOn(apvts.getRawParameterValue("tubeOn")->load() > 0.5f);
    amp.setSlapOn(apvts.getRawParameterValue("slap")->load() > 0.5f);
    amp.setBassDb(apvts.getRawParameterValue("ampBass")->load());
    amp.setMidDb(apvts.getRawParameterValue("ampMid")->load());
    amp.setTrebleDb(apvts.getRawParameterValue("ampTreble")->load());

    // LowCut
    bool lcOn = apvts.getRawParameterValue("lowCutOn")->load() > 0.5f;
    if (lcOn != lastLowCutOn) {
      dspChain.getLowCut().reset();
      lastLowCutOn = lcOn;
    }
    dspChain.setLowCutBypassed(!lcOn);

    // Auto-Gain
    bool ampAutoGain = apvts.getRawParameterValue("ampAutoGain")->load() > 0.5f;
    amp.setAutoGain(ampAutoGain);

    // Note: ampVolume is post-overdrive channel volume, summed into OutputGain.
  }

  // 4. Compressor
  {
    auto &comp = dspChain.getCompressor();

    const bool compOn = apvts.getRawParameterValue("compOn")->load() > 0.5f;
    comp.setCompOn(compOn);
    comp.setThresholdDb(apvts.getRawParameterValue("compThresh")->load());
    comp.setRatioIndex((int)apvts.getRawParameterValue("compRatio")->load());
    comp.setAttackMs(apvts.getRawParameterValue("compAttack")->load());
    comp.setReleaseMs(apvts.getRawParameterValue("compRelease")->load());
    comp.setMakeupGainDb(apvts.getRawParameterValue("compMakeup")->load());

    // Auto-Makeup (bounded inside compressor, safe)
    const bool compAutoMakeup =
        apvts.getRawParameterValue("compAutoMakeup")->load() > 0.5f;
    comp.setAutoMakeup(compAutoMakeup);

    // Bass sidechain HPF: prevents low-end pumping (slap tends to like a bit
    // higher)

    // "leichtes B" character: tie to Tube Saturation toggle (subtle, not fuzz)
    const bool tubeOn = apvts.getRawParameterValue("tubeOn")->load() > 0.5f;
  }

  // 5. ModFX
  {
    auto &mod = dspChain.getModFX();
    mod.setPhaserOn(apvts.getRawParameterValue("phaserOn")->load() > 0.5f);
    mod.setPhaserRate(apvts.getRawParameterValue("phRate")->load());
    mod.setPhaserMix(apvts.getRawParameterValue("phMix")->load() / 100.0f);
    mod.setPhaserColour(apvts.getRawParameterValue("phColour")->load());

    mod.setChorusOn(apvts.getRawParameterValue("chorusOn")->load() > 0.5f);
    mod.setChorusRate(apvts.getRawParameterValue("chRate")->load());
    mod.setChorusDepth(apvts.getRawParameterValue("chDepth")->load());
    mod.setChorusMix(apvts.getRawParameterValue("chMix")->load() / 100.0f);

    mod.setParallel(apvts.getRawParameterValue("fxParallel")->load() > 0.5f);
  }

  // 6. Mojo
  {
    // Drive derived from Amp Gain + Comp Input?
    float compInDb = apvts.getRawParameterValue("compInput")->load();
    float ampGainDb = apvts.getRawParameterValue("ampGain")->load();
    float g01 = (ampGainDb + compInDb) / 24.0f;
    dspChain.getMojo().setMojoDrive01(
        juce::jlimit(0.0f, 1.0f, 0.20f + 0.80f * g01));
  }

  // 7. CabSim
  {
    auto &cab = dspChain.getCabSim();
    cab.setCabType((int)apvts.getRawParameterValue("cabType")->load());
    cab.setMix(apvts.getRawParameterValue("irMix")->load() / 100.0f);
  }

  // 8. Output Gain + Master Mix
  {
    float masterDb = apvts.getRawParameterValue("masterOut")->load();
    float ampVolDb = apvts.getRawParameterValue("ampVolume")->load();
    // Apply Amp Volume + Master Volume at the end stage
    auto &out = dspChain.getOutputGain();
    out.setGainDecibels(masterDb + ampVolDb);
    out.setSafetyClipThreshold(0.99f);

    out.setMonoMakerFreq(apvts.getRawParameterValue("monoMaker")->load());
    bool mmOn = apvts.getRawParameterValue("monoMakerOn")->load() > 0.5f;
    if (mmOn != lastMonoMakerOn) {
      out.reset();
      lastMonoMakerOn = mmOn;
    }
    out.setMonoMakerEnabled(mmOn);
    out.setAutoGain(apvts.getRawParameterValue("autoGain")->load() > 0.5f);
  }

  // ===== PROCESS =====
  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::ProcessContextReplacing<float> ctx(block);

  // Capture Dry for Mix
  dryBuffer.makeCopyOf(buffer, true);

  // Central DSP Chain
  {
    const auto startTime = juce::Time::getHighResolutionTicks();

    dspChain.process(ctx);

    const auto endTime = juce::Time::getHighResolutionTicks();
    const double elapsed =
        juce::Time::highResolutionTicksToSeconds(endTime - startTime);
    const double totalAvailable = buffer.getNumSamples() / getSampleRate();

    if (totalAvailable > 0) {
      float usage = (float)(elapsed / totalAvailable);
      cpuUsage.store(cpuUsage.load() * 0.98f + usage * 0.02f,
                     std::memory_order_relaxed);
    }
  }

  // Apply Mix (Dry/Wet)
  // Note: OutputGain is already applied in dspChain.process (at end).
  // Dry signal is totally dry (pre OutputGain).
  // Wet signal is processed (with OutputGain).
  // Blending them effectively means "Dry" is unaffected by Master Volume.
  // This is acceptable behavior for "Mix".

  float mixVal = apvts.getRawParameterValue("masterMix")->load() / 100.0f;
  if (mixVal < 0.999f) {
    const float wetGain = std::sqrt(mixVal);
    const float dryGain = std::sqrt(1.0f - mixVal); // Equal power crossfade

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
      auto *w = buffer.getWritePointer(ch);
      auto *d = dryBuffer.getReadPointer(ch);
      for (int i = 0; i < buffer.getNumSamples(); ++i) {
        w[i] = w[i] * wetGain + d[i] * dryGain;
      }
    }
  }

  // Tuner is visual only (no mute)
}

//==============================================================================
void FunkyMooseAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  state.setProperty("version", projectVersion, nullptr);
  state.setProperty("customIrPath", dspChain.getCabSim().customIrPath, nullptr);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void FunkyMooseAudioProcessor::setStateInformation(const void *data,
                                                   int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr) {
    if (xmlState->hasTagName(apvts.state.getType())) {
      auto vt = juce::ValueTree::fromXml(*xmlState);
      int loadedVersion = vt.getProperty("version", 0);

      juce::String irPath = vt.getProperty("customIrPath", "");
      if (irPath.isNotEmpty()) {
        dspChain.getCabSim().loadCustomIr(irPath);
      }

      apvts.replaceState(vt);
    }
  }
}

juce::File FunkyMooseAudioProcessor::getPresetsFolder() {
  juce::File appData =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
  juce::File folder = appData.getChildFile("Application Support")
                          .getChildFile("FunkyMooseAmp")
                          .getChildFile("Presets");
  if (!folder.exists())
    folder.createDirectory();
  return folder;
}

juce::StringArray FunkyMooseAudioProcessor::getPresetList() {
  juce::StringArray list;
  list.add("F:Default");
  list.add("F:Birdland `s Pleasure");
  list.add("F:James`s Soul");
  list.add("F:King Slap");
  list.add("F:Play the Chords");
  list.add("F:Sledge the Hammer");
  list.add("F:Synthish");
  list.add("F:in the Name of PJ");
  list.add("F:on the One");

  auto folder = getPresetsFolder();
  juce::Array<juce::File> files;
  folder.findChildFiles(files, juce::File::findFiles, false, "*.xml");
  for (auto &f : files)
    list.add("U:" + f.getFileNameWithoutExtension());

  return list;
}

void FunkyMooseAudioProcessor::loadPreset(const juce::String &presetName) {
  for (auto &p : getParameters())
    if (auto *rp = dynamic_cast<juce::RangedAudioParameter *>(p))
      rp->setValueNotifyingHost(rp->getDefaultValue());

  auto setVal = [&](const juce::String &id, float val) {
    if (auto *p = apvts.getParameter(id))
      p->setValueNotifyingHost(p->convertTo0to1(val));
  };
  auto setBool = [&](const juce::String &id, bool val) {
    if (auto *p = apvts.getParameter(id))
      p->setValueNotifyingHost(val ? 1.0f : 0.0f);
  };

  if (presetName == "Birdland `s Pleasure") {
    setVal("ampBass", 8.046817779541016f);
    setVal("ampGain", 12.35122776031494f);
    setVal("ampMid", 8.056770324707031f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 3.675234317779541f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 33.86650466918945f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -21.31538200378418f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 65.0f);
    setVal("masterOut", 0.2981683015823364f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 1.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 0.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "Default") {
    setVal("ampBass", 6.526051998138428f);
    setVal("ampGain", 8.781200408935547f);
    setVal("ampMid", -1.478480100631714f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 5.612707614898682f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -6.69426441192627f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 65.0f);
    setVal("masterOut", -4.289459228515625f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 0.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 218.8702545166016f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 0.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 0.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "James`s Soul") {
    setVal("ampBass", 6.709151744842529f);
    setVal("ampGain", 8.781200408935547f);
    setVal("ampMid", 0.6908397674560547f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", -7.996444225311279f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 33.86650466918945f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -15.04234313964844f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 65.0f);
    setVal("masterOut", -4.289459228515625f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 1.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 0.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 0.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "King Slap") {
    setVal("ampBass", -0.2784998416900635f);
    setVal("ampGain", 8.781200408935547f);
    setVal("ampMid", -4.831964492797852f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 12.0f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 33.86650466918945f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -15.04234313964844f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 65.0f);
    setVal("masterOut", -4.289459228515625f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 1.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 0.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 218.8702545166016f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 0.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 0.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "Play the Chords") {
    setVal("ampBass", -4.182948589324951f);
    setVal("ampGain", 12.35122776031494f);
    setVal("ampMid", 10.17400932312012f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 12.0f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 84.42182922363281f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -21.31538200378418f);
    setVal("envAttack", 0.1865661293268204f);
    setVal("envDecay", 0.3568983972072601f);
    setVal("envOn", 0.0f);
    setVal("envRange", 100.0f);
    setVal("masterOut", -4.726724624633789f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 14.58043003082275f);
    setVal("octModern", 0.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.5108248591423035f);
    setVal("phMix", 84.7347640991211f);
    setVal("phRate", 0.08680211007595062f);
    setVal("phaserOn", 1.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "Sledge the Hammer") {
    setVal("ampBass", 2.581031799316406f);
    setVal("ampGain", 12.35122776031494f);
    setVal("ampMid", 1.432588577270508f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 6.362621784210205f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 33.86650466918945f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -21.31538200378418f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 65.0f);
    setVal("masterOut", 0.2981683015823364f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 1.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 0.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "Synthish") {
    setVal("ampBass", 2.581031799316406f);
    setVal("ampGain", 12.35122776031494f);
    setVal("ampMid", 8.117626190185547f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 12.0f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 14.87058067321777f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -21.31538200378418f);
    setVal("envAttack", 0.1865661293268204f);
    setVal("envDecay", 0.3568983972072601f);
    setVal("envOn", 1.0f);
    setVal("envRange", 100.0f);
    setVal("masterOut", -4.726724624633789f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 14.58043003082275f);
    setVal("octModern", 0.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 0.5108248591423035f);
    setVal("phMix", 49.02029800415039f);
    setVal("phRate", 0.08680211007595062f);
    setVal("phaserOn", 1.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "in the Name of PJ") {
    setVal("ampBass", 3.449965953826904f);
    setVal("ampGain", 24.0f);
    setVal("ampMid", 7.721807956695557f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 3.353964328765869f);
    setVal("ampVolume", 6.0f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 84.42182922363281f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", 0.8168506622314453f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 2.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -29.32355117797852f);
    setVal("envAttack", 0.1865661293268204f);
    setVal("envDecay", 0.3568983972072601f);
    setVal("envOn", 0.0f);
    setVal("envRange", 100.0f);
    setVal("masterOut", -25.07347869873047f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 14.58043003082275f);
    setVal("octModern", 0.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.5108248591423035f);
    setVal("phMix", 84.7347640991211f);
    setVal("phRate", 0.08680211007595062f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  if (presetName == "on the One") {
    setVal("ampBass", 2.581031799316406f);
    setVal("ampGain", 12.35122776031494f);
    setVal("ampMid", 8.117626190185547f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 12.0f);
    setVal("ampVolume", -1.000000476837158f);
    setVal("chDepth", 0.2512639164924622f);
    setVal("chMix", 33.86650466918945f);
    setVal("chRate", 0.1234697178006172f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", -5.961386203765869f);
    setVal("compMakeup", 4.130940437316895f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.6371078491211f);
    setVal("compThresh", -21.31538200378418f);
    setVal("envAttack", 0.1865661293268204f);
    setVal("envDecay", 0.3568983972072601f);
    setVal("envOn", 1.0f);
    setVal("envRange", 100.0f);
    setVal("masterOut", 0.2981683015823364f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 51.86192321777344f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 0.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 207.9331970214844f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    return;
  }

  auto file = getPresetsFolder().getChildFile(presetName + ".xml");
  if (file.existsAsFile()) {
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
      apvts.replaceState(juce::ValueTree::fromXml(*xml));
  }
}

void FunkyMooseAudioProcessor::savePreset(const juce::String &presetName) {
  auto file = getPresetsFolder().getChildFile(presetName + ".xml");
  auto state = apvts.copyState();
  state.setProperty("version", projectVersion, nullptr);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  xml->writeTo(file);
}

void FunkyMooseAudioProcessor::loadFactoryPresets() {}
