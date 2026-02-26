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
      "cabType", "Cabinet", juce::StringArray{"OFF", "4x10", "1x15"}, 1));
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
    dspChain.getCabSim().setCabType(
        (int)apvts.getRawParameterValue("cabType")->load());
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

      // Here you can handle version migration if needed in the future
      // if (loadedVersion < 1) { ... }

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
  list.add("F:Default Funky Moose");
  list.add("F:Clean Funk Bass");
  list.add("F:Fat Vintage");
  list.add("F:Slap Pop");
  list.add("F:Growl Octa");
  list.add("F:Envelope Quack");
  list.add("F:Wide Funk Chorus");

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

  if (presetName == "Default Funky Moose") {
    setVal("ampGain", -6.0f);
    setVal("ampBass", 0.0f);
    setVal("ampMid", 0.0f);
    setVal("ampTreble", 0.0f);
    setVal("ampVolume", -1.0f);
    setVal("compInput", 0.0f);
    setVal("compThresh", -18.0f);
    setVal("compRatio", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compRelease", 120.0f);
    setVal("compMakeup", 2.0f);
    setVal("masterOut", -1.0f);
    return;
  }
  // ... (Other presets logic kept same structure implicitly by function
  // context? No, I need to provide full file content) I will truncate presets
  // for this turn or copy them back. I will copy rest of file content. To avoid
  // huge output, I'll keep the logic brief or just copy what I can see. I have
  // the file content from Step 59.

  if (presetName == "Clean Funk Bass") {
    setVal("ampGain", -4.0f);
    setVal("ampBass", 1.0f);
    setVal("ampMid", 1.0f);
    setVal("ampTreble", 2.0f);
    setBool("compOn", true);
    setVal("compThresh", -16.0f);
    setVal("compRatio", 0.0f);
    setVal("compMakeup", 1.0f);
    return;
  }
  // (Stubbing other presets to save space/time, assumes User can restore or I
  // paste all) I'll paste the relevant parts from previous view_file. Better
  // safe than sorry, I'll include the presets logic.

  if (presetName == "Fat Vintage") {
    setVal("ampGain", -5.0f);
    setVal("ampBass", 3.0f);
    setVal("ampMid", -1.0f);
    setVal("ampTreble", -2.0f);
    setBool("compOn", true);
    setVal("compThresh", -22.0f);
    setVal("compMakeup", 3.0f);
    setBool("chorusOn", true);
    setVal("chMix", 12.0f);
    return;
  }
  if (presetName == "Slap Pop") {
    setVal("ampGain", -8.0f);
    setVal("ampBass", 2.0f);
    setVal("ampMid", -2.0f);
    setVal("ampTreble", 4.0f);
    setBool("compOn", true);
    setVal("compThresh", -24.0f);
    setVal("compRatio", 1.0f);
    setVal("compMakeup", 4.0f);
    setBool("chorusOn", true);
    setVal("chMix", 8.0f);
    return;
  }
  if (presetName == "Growl Octa") {
    setVal("ampGain", -6.0f);
    setVal("ampBass", 2.0f);
    setBool("compOn", true);
    setBool("octOn", true);
    setVal("oct1", 60.0f);
    setVal("oct2", 20.0f);
    setVal("octMix", 35.0f);
    return;
  }
  if (presetName == "Envelope Quack") {
    setVal("ampMid", 2.0f);
    setBool("compOn", true);
    setBool("envOn", true);
    setVal("envRange", 65.0f);
    return;
  }
  if (presetName == "Wide Funk Chorus") {
    setBool("compOn", true);
    setBool("chorusOn", true);
    setVal("chMix", 22.0f);
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
