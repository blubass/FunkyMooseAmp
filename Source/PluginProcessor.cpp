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
      "ampBass", "Bass", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
  p.push_back(std::make_unique<APF>(
      "ampMid", "Mid", juce::NormalisableRange<float>(-18.0f, 18.0f), 0.0f));
  p.push_back(std::make_unique<APF>(
      "ampTreble", "Treble", juce::NormalisableRange<float>(-18.0f, 18.0f),
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

  // Cache Parameters for high-performance retrieval
  bypassParam = apvts.getRawParameterValue("bypass");
  autoGateParam = apvts.getRawParameterValue("autoGate");
  octOnParam = apvts.getRawParameterValue("octOn");
  oct1Param = apvts.getRawParameterValue("oct1");
  oct2Param = apvts.getRawParameterValue("oct2");
  octMixParam = apvts.getRawParameterValue("octMix");
  envOnParam = apvts.getRawParameterValue("envOn");
  envDecayParam = apvts.getRawParameterValue("envDecay");
  envRangeParam = apvts.getRawParameterValue("envRange");
  ampOnParam = apvts.getRawParameterValue("ampOn");
  tubeOnParam = apvts.getRawParameterValue("tubeOn");
  slapParam = apvts.getRawParameterValue("slap");
  ampBassParam = apvts.getRawParameterValue("ampBass");
  ampMidParam = apvts.getRawParameterValue("ampMid");
  ampTrebleParam = apvts.getRawParameterValue("ampTreble");
  ampVolumeParam = apvts.getRawParameterValue("ampVolume");
  ampGainParam = apvts.getRawParameterValue("ampGain");
  ampAutoGainParam = apvts.getRawParameterValue("ampAutoGain");
  lowCutOnParam = apvts.getRawParameterValue("lowCutOn");
  compOnParam = apvts.getRawParameterValue("compOn");
  compInputParam = apvts.getRawParameterValue("compInput");
  compThreshParam = apvts.getRawParameterValue("compThresh");
  compMakeupParam = apvts.getRawParameterValue("compMakeup");
  compRatioParam = apvts.getRawParameterValue("compRatio");
  compAttackParam = apvts.getRawParameterValue("compAttack");
  compReleaseParam = apvts.getRawParameterValue("compRelease");
  compAutoMakeupParam = apvts.getRawParameterValue("compAutoMakeup");
  punchParam = apvts.getRawParameterValue("punch");
  phaserOnParam = apvts.getRawParameterValue("phaserOn");
  phRateParam = apvts.getRawParameterValue("phRate");
  phMixParam = apvts.getRawParameterValue("phMix");
  phColourParam = apvts.getRawParameterValue("phColour");
  chorusOnParam = apvts.getRawParameterValue("chorusOn");
  chRateParam = apvts.getRawParameterValue("chRate");
  chDepthParam = apvts.getRawParameterValue("chDepth");
  chMixParam = apvts.getRawParameterValue("chMix");
  fxParallelParam = apvts.getRawParameterValue("fxParallel");
  cabTypeParam = apvts.getRawParameterValue("cabType");
  irMixParam = apvts.getRawParameterValue("irMix");
  masterOutParam = apvts.getRawParameterValue("masterOut");
  monoMakerParam = apvts.getRawParameterValue("monoMaker");
  monoMakerOnParam = apvts.getRawParameterValue("monoMakerOn");
  autoGainParam = apvts.getRawParameterValue("autoGain");

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
  // Standalone Smart Mono Summing (prevents 6dB drop if only one input is used,
  // but avoids +6dB boost if both are used)
  else if (wrapperType == juce::AudioProcessor::wrapperType_Standalone &&
           buffer.getNumChannels() >= 2) {
    const int n = buffer.getNumSamples();
    float lMag = buffer.getMagnitude(0, 0, n);
    float rMag = buffer.getMagnitude(1, 0, n);

    // If both have significant signal (> -60dB approx), sum with 0.5.
    // Otherwise, sum with 1.0 to keep unity gain for the active channel.
    float smartGain = (lMag > 0.001f && rMag > 0.001f) ? 0.5f : 1.0f;

    auto *L = buffer.getWritePointer(0);
    auto *R = buffer.getWritePointer(1);
    for (int i = 0; i < n; ++i) {
      float mono = (L[i] + R[i]) * smartGain;
      L[i] = mono;
      R[i] = mono;
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

    // Same smart-gain logic for tuner to avoid "level trap"
    float lMag = buffer.getMagnitude(0, 0, n);
    float rMag =
        (buffer.getNumChannels() > 1) ? buffer.getMagnitude(1, 0, n) : 0.0f;
    float tunerSumGain = (lMag > 0.001f && rMag > 0.001f) ? 0.5f : 1.0f;

    for (int i = 0; i < n; ++i)
      m[i] = (L[i] + (R ? R[i] : 0.0f)) * tunerSumGain;

    tunerFifo.push(m, n);
  }

  if (!mState.prepared)
    prepareToPlay(getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                  buffer.getNumSamples());

  // HARD DSP BYPASS
  if (bypassParam->load() > 0.5f) {
    return;
  }

  // ===== UPDATE PARAMETERS =====

  // 1. Octaver / Env
  {
    auto &octEnv = dspChain.getOctEnv();
    const bool octOn = octOnParam->load() > 0.5f;
    const bool envOn = envOnParam->load() > 0.5f;

    const bool anyActive = octOn || envOn;
    dspChain.setBypassed<1>(!anyActive);

    octEnv.setOctaveOn(octOn);
    octEnv.setEnvelopeOn(envOn);

    octEnv.setOctave1(oct1Param->load() / 100.0f);
    octEnv.setOctave2(oct2Param->load() / 100.0f);
    octEnv.setOctaveMix(octMixParam->load() / 100.0f);
    octEnv.setEnvDecay(envDecayParam->load());
    octEnv.setEnvRange(envRangeParam->load() / 100.0f);
  }

  // 1.5 Smart Gate
  {
    const bool gateOn = autoGateParam->load() > 0.5f;
    dspChain.setBypassed<0>(!gateOn);
    dspChain.getSmartGate().setEnabled(gateOn);
  }

  // 2. Input Gain (Drive) + Comp Input
  {
    float compInDb = compInputParam->load();
    float ampGainDb = ampGainParam->load();
    dspChain.getInputGain().setGainDecibels(compInDb + ampGainDb);
  }

  // 3. Amp / Tone
  {
    auto &amp = dspChain.getAmpTone();
    amp.setAmpOn(ampOnParam->load() > 0.5f);
    amp.setTubeOn(tubeOnParam->load() > 0.5f);
    amp.setSlapOn(slapParam->load() > 0.5f);
    amp.setBassDb(ampBassParam->load());
    amp.setMidDb(ampMidParam->load());
    amp.setTrebleDb(ampTrebleParam->load());

    // LowCut
    const bool lcOn = lowCutOnParam->load() > 0.5f;
    dspChain.setBypassed<2>(!lcOn);

    // Auto-Gain
    amp.setAutoGain(ampAutoGainParam->load() > 0.5f);
  }

  // 4. Compressor
  {
    auto &comp = dspChain.getCompressor();
    const bool compOn = compOnParam->load() > 0.5f;

    dspChain.setBypassed<5>(!compOn);

    comp.setCompOn(compOn);
    comp.setThresholdDb(compThreshParam->load());
    comp.setRatioIndex((int)compRatioParam->load());
    comp.setAttackMs(compAttackParam->load());
    comp.setReleaseMs(compReleaseParam->load());
    comp.setMakeupGainDb(compMakeupParam->load());
    comp.setAutoMakeup(compAutoMakeupParam->load() > 0.5f);

    const bool punch = punchParam->load() > 0.5f;
    comp.setPunch(punch);
    punchEnabledForUI.store(punch);
  }

  // 5. ModFX
  {
    auto &mod = dspChain.getModFX();
    const bool phOn = phaserOnParam->load() > 0.5f;
    const bool chOn = chorusOnParam->load() > 0.5f;

    const bool anyModActive = phOn || chOn;
    dspChain.setBypassed<6>(!anyModActive);

    mod.setPhaserOn(phOn);
    mod.setPhaserRate(phRateParam->load());
    mod.setPhaserMix(phMixParam->load() / 100.0f);
    mod.setPhaserColour(phColourParam->load());

    mod.setChorusOn(chOn);
    mod.setChorusRate(chRateParam->load());
    mod.setChorusDepth(chDepthParam->load());
    mod.setChorusMix(chMixParam->load() / 100.0f);

    mod.setParallel(fxParallelParam->load() > 0.5f);
  }

  // 6. Mojo
  {
    auto &mojo = dspChain.getMojo();
    const bool mojoTargetActive =
        (ampOnParam->load() > 0.5f) && (tubeOnParam->load() > 0.5f);

    dspChain.setBypassed<7>(!mojoTargetActive);

    if (mojoTargetActive) {
      float g01 = (ampGainParam->load() + compInputParam->load()) / 24.0f;
      mojo.setMojoDrive01(juce::jlimit(0.0f, 1.0f, 0.20f + 0.80f * g01));
    }
  }

  // 7. CabSim
  {
    auto &cab = dspChain.getCabSim();
    cab.setCabType((int)cabTypeParam->load());
    cab.setMix(irMixParam->load() / 100.0f);
  }

  // 8. Output Gain + Master Mix
  {
    auto &out = dspChain.getOutputGain();
    out.setGainDecibels(masterOutParam->load() + ampVolumeParam->load());
    out.setSafetyClipThreshold(0.99f);

    out.setMonoMakerFreq(monoMakerParam->load());
    bool mmOn = monoMakerOnParam->load() > 0.5f;
    out.setMonoMakerEnabled(mmOn);
    out.setAutoGain(autoGainParam->load() > 0.5f);
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
  list.add("F:Jaco Bridge");
  list.add("F:Miller Slap");
  list.add("F:Jamerson Warmth");
  list.add("F:Palladino P-Bass");
  list.add("F:Bootsy Power");
  list.add("F:Mark King King");
  list.add("F:Flea Aggression");
  list.add("F:Geddy Grit");
  list.add("F:Doom Moose");
  list.add("F:Motown 15\"");
  list.add("F:Prog Fusion");

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

  if (presetName == "Default") {
    // Neutral starting point
    setVal("ampGain", 8.0f);
    setVal("ampBass", 0.0f);
    setVal("ampMid", 0.0f);
    setVal("ampTreble", 0.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -18.0f);
    setVal("compRatio", 0.0f); // 4:1
    setVal("cabType", 0.0f);   // 4x12
    setVal("masterOut", 0.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Jaco Bridge") {
    setVal("ampGain", 11.0f); // Moderate drive for that growl
    setVal("ampBass", -3.0f);
    setVal("ampMid", 12.0f);
    setVal("ampTreble", 2.0f);
    setVal("tubeOn", 1.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -22.0f);
    setVal("compRatio", 1.0f);
    setVal("cabType", 1.0f); // 4x10
    setVal("masterOut", 0.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Miller Slap") {
    setVal("ampGain", 8.0f);
    setVal("ampBass", 7.0f);
    setVal("ampMid", -8.0f);
    setVal("ampTreble", 10.0f);
    setVal("slap", 1.0f);
    setVal("punch", 1.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -20.0f);
    setVal("compRatio", 2.0f);
    setVal("cabType", 1.0f);
    setVal("masterOut", -1.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Jamerson Warmth") {
    setVal("ampGain", 4.0f); // Clean
    setVal("ampBass", 5.0f);
    setVal("ampMid", 2.0f);
    setVal("ampTreble", -12.0f);
    setVal("tubeOn", 1.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -14.0f);
    setVal("compRatio", 0.0f);
    setVal("cabType", 2.0f); // 15"
    setVal("masterOut", 3.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Palladino P-Bass") {
    setVal("ampGain", 6.0f);
    setVal("ampBass", 4.0f);
    setVal("ampMid", 2.0f);
    setVal("ampTreble", -3.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -18.0f);
    setVal("compRatio", 0.0f);
    setVal("compAttack", 50.0f);
    setVal("compRelease", 250.0f);
    setVal("cabType", 2.0f);
    setVal("masterOut", 2.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Bootsy Power") {
    setVal("ampGain", 12.0f);
    setVal("envOn", 1.0f);
    setVal("envRange", 85.0f);
    setVal("envAttack", 25.0f);
    setVal("phaserOn", 1.0f);
    setVal("phMix", 50.0f);
    setVal("phRate", 0.2f);
    setVal("fxParallel", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterOut", -1.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Mark King King") {
    setVal("ampGain", 10.0f);
    setVal("ampBass", 5.0f);
    setVal("ampMid", -5.0f);
    setVal("ampTreble", 12.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -25.0f);
    setVal("compRatio", 3.0f);
    setVal("compAttack", 1.0f);
    setVal("chorusOn", 1.0f);
    setVal("chMix", 25.0f);
    setVal("cabType", 1.0f);
    setVal("masterOut", -2.5f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Flea Aggression") {
    setVal("ampGain", 20.0f); // Heavy drive (Mojo kicks in)
    setVal("ampBass", 6.0f);
    setVal("ampMid", 2.0f);
    setVal("ampTreble", 9.0f);
    setVal("tubeOn", 1.0f);
    setVal("punch", 1.0f);
    setVal("cabType", 0.0f); // 4x12
    setVal("masterOut", -7.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Geddy Grit") {
    setVal("ampGain", 24.0f); // Max Grit
    setVal("ampBass", 3.0f);
    setVal("ampMid", 7.0f);
    setVal("ampTreble", 7.0f);
    setVal("tubeOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("monoMaker", 220.0f); // Keeps low end solid
    setVal("cabType", 0.0f);
    setVal("masterOut", -12.0f);
    setVal("ampVolume", 5.0f);
    return;
  }

  if (presetName == "Doom Moose") {
    setVal("ampGain", 24.0f);
    setVal("ampBass", 8.0f);
    setVal("ampMid", 4.0f);
    setVal("ampTreble", -2.0f);
    setVal("octOn", 1.0f);
    setVal("oct1", 70.0f);
    setVal("octMix", 40.0f); // More sub!
    setVal("tubeOn", 1.0f);
    setVal("cabType", 2.0f); // 15"
    setVal("masterOut", -10.0f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Motown 15\"") {
    setVal("ampGain", 3.0f);
    setVal("ampBass", 7.0f);
    setVal("ampMid", 2.0f);
    setVal("ampTreble", -15.0f);
    setVal("tubeOn", 1.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -10.0f);
    setVal("cabType", 2.0f);
    setVal("masterOut", 3.5f);
    setVal("ampVolume", 0.0f);
    return;
  }

  if (presetName == "Prog Fusion") {
    setVal("ampGain", 10.0f);
    setVal("ampBass", 3.0f);
    setVal("ampMid", 5.0f);
    setVal("ampTreble", 6.0f);
    setVal("compOn", 1.0f);
    setVal("compThresh", -22.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("chOn", 1.0f);
    setVal("chMix", 18.0f);
    setVal("fxParallel", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterOut", -1.0f);
    setVal("ampVolume", 0.0f);
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
