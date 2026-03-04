#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
FunkyMooseAudioProcessor::FunkyMooseAudioProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParams()) {
  for (auto *param : getParameters()) {
    if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param)) {
      apvts.addParameterListener(p->paramID, this);
    }
  }

  // Realtime-safe MIDI map init
  for (auto &v : ccToParamIndex)
    v.store(-1);

  parameters.clear();
  parameterIDs.clear();
  parameters.reserve(this->getParameters().size());
  parameterIDs.reserve(this->getParameters().size());

  for (auto *p : this->getParameters()) {
    if (auto *rp = dynamic_cast<juce::RangedAudioParameter *>(p)) {
      parameters.push_back(rp);

      if (auto *withID = dynamic_cast<juce::AudioProcessorParameterWithID *>(p))
        parameterIDs.push_back(withID->paramID);
      else
        parameterIDs.push_back(p->getName(64));
    }
  }
  loadPreset("Default");
  loadMidiMap();
}

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
  p.push_back(std::make_unique<APB>(
      "forceMonoInput", "Force Mono Input (Standalone Mode)", true));

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
      3.0f));
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
  p.push_back(std::make_unique<APB>("masterOn", "Master On", true));
  p.push_back(std::make_unique<APF>(
      "masterOut", "Output", juce::NormalisableRange<float>(-60.0f, 6.0f),
      0.0f));
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

  // Pre-allocate tuner scratch buffer (no allocations in audio thread)
  tunerScratchMono.setSize(1, samplesPerBlock, false, false, true);
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
  masterOnParam = apvts.getRawParameterValue("masterOn");
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
  forceMonoInputParam = apvts.getRawParameterValue("forceMonoInput");

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

  // Standalone: Allow disabled input or output (very flexible)
  if (wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
    // Only reject if OUTPUT is disabled (we need somewhere to send audio!)
    if (out.isDisabled())
      return false;
    // Input can be disabled (e.g., output-only mode or input not configured)
    return true;
  }

  // Plugin mode: Both buses must be enabled
  if (in.isDisabled() || out.isDisabled())
    return false;

  // Mono/Stereo standard checks for plugins
  if (out == juce::AudioChannelSet::mono() ||
      out == juce::AudioChannelSet::stereo()) {
    if (in == juce::AudioChannelSet::mono() ||
        in == juce::AudioChannelSet::stereo())
      return true;
  }

  return in == out;
}

void FunkyMooseAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                            juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  // MIDI REMOTE CONTROL: Handle incoming CC messages
  bool hasMidi = false;
  for (const auto metadata : midiMessages) {
    auto msg = metadata.getMessage();
    if (msg.isController() || msg.isNoteOn() || msg.isNoteOff() ||
        msg.isProgramChange()) {
      hasMidi = true;
      if (msg.isNoteOn() || msg.isNoteOff()) {
        lastMidiType.store(1);
        lastMidiNumber.store(msg.getNoteNumber());
      } else if (msg.isController()) {
        lastMidiType.store(2);
        lastMidiNumber.store(msg.getControllerNumber());
      }
    }

    if (msg.isController()) {
      const int ccNum = msg.getControllerNumber();
      const float ccVal = (float)msg.getControllerValue() / 127.0f;

      // CUSTOM MIDI LEARN MAPPING (realtime-safe)
      bool handledByCustomMap = false;

      // If learn is armed and we already captured which parameter to learn,
      // the next received CC will bind to it.
      if (const int learnIdx = learningParamIndex.load(); learnIdx >= 0) {
        if (ccNum >= 0 && ccNum < 128) {
          ccToParamIndex[(size_t)ccNum].store(learnIdx);
          learnedCC.store(ccNum);
          midiMapDirty.store(true);
          triggerAsyncUpdate(); // save on message thread
        }

        learningParamIndex.store(-1);
      }

      const int mappedIndex = (ccNum >= 0 && ccNum < 128)
                                  ? ccToParamIndex[(size_t)ccNum].load()
                                  : -1;

      if (mappedIndex >= 0 && mappedIndex < (int)parameters.size()) {
        if (auto *p = parameters[(size_t)mappedIndex])
          p->setValueNotifyingHost(ccVal);

        handledByCustomMap = true;
      }

      if (handledByCustomMap)
        continue;

      // AKAI MPK MINI MAPPING (User Config)
      // K1-K8: CC 2-9
      if (ccNum == 2) { // K1: Gain
        if (auto *p = apvts.getParameter("ampGain"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 3) { // K2: Bass
        if (auto *p = apvts.getParameter("ampBass"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 4) { // K3: Mid
        if (auto *p = apvts.getParameter("ampMid"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 5) { // K4: Treble
        if (auto *p = apvts.getParameter("ampTreble"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 6) { // K5: Amp Volume
        if (auto *p = apvts.getParameter("ampVolume"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 7) { // K6: Octave Mix
        if (auto *p = apvts.getParameter("octMix"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 8) { // K7: Envelope Range
        if (auto *p = apvts.getParameter("envRange"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 9) { // K8: Phaser Mix
        if (auto *p = apvts.getParameter("phMix"))
          p->setValueNotifyingHost(ccVal);
      }
      // LEGACY / DAW MAPPING
      else if (ccNum == 1) { // Modwheel
        if (auto *p = apvts.getParameter("envRange"))
          p->setValueNotifyingHost(ccVal);
      } else if (ccNum == 74) { // Standard Filter Cutoff
        if (auto *p = apvts.getParameter("phMix"))
          p->setValueNotifyingHost(ccVal);
      }
    } else if (msg.isNoteOn()) {
      const int noteNum = msg.getNoteNumber();

      // AKAI MPK MINI PAD MAPPING (Toggles)
      // Top Row (44-47), Bottom Row (48-51)
      auto toggleParam = [&](const juce::String &paramID) {
        if (auto *p = apvts.getParameter(paramID)) {
          float currentVal = p->getValue();
          p->setValueNotifyingHost(currentVal > 0.5f ? 0.0f : 1.0f);
        }
      };

      if (noteNum == 44)
        toggleParam("ampOn");
      else if (noteNum == 45)
        toggleParam("octOn");
      else if (noteNum == 46)
        toggleParam("envOn");
      else if (noteNum == 47)
        toggleParam("phaserOn");
      else if (noteNum == 48)
        toggleParam("chorusOn");
      else if (noteNum == 49)
        toggleParam("compOn");
      else if (noteNum == 50)
        toggleParam("cabType"); // Cycle or toggle cab
      else if (noteNum == 51)
        toggleParam("bypass");
    }
  }

  if (hasMidi) {
    midiActivity.store(1.0f, std::memory_order_relaxed);
  }

  const int totalIns = getTotalNumInputChannels();
  const int totalOuts = getTotalNumOutputChannels();
  const int bufferChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  // DEBUG: Log audio device info on first call (Safe for MSVC)
  static bool firstCall = true;
  if (firstCall) {
    firstCall = false;
    DBG("===== AUDIO PROCESSOR INITIALIZED =====");
    DBG("Total Input Channels: " + juce::String(totalIns));
    DBG("Total Output Channels: " + juce::String(totalOuts));
    DBG("Buffer Channels: " + juce::String(bufferChannels));
    DBG("Sample Rate: " + juce::String(getSampleRate()));
    DBG("Wrapper Type: " + juce::String((int)wrapperType));
    DBG("Is Standalone: " +
        juce::String(
            (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
                ? "Yes"
                : "No"));
  }

  // Safety: If the host changed channel counts or sample rate without calling
  // prepareToPlay, we MUST re-prepare now to avoid crashes (especially with
  // dryBuffer and oversampling).
  if (!mState.prepared || (int)mState.spec.numChannels < bufferChannels ||
      mState.spec.maximumBlockSize < (juce::uint32)numSamples) {
    prepareToPlay(getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                  numSamples);
  }

  // Safety check to ensure we don't process with 0 channels (MSVC robustness)
  if (bufferChannels == 0 || numSamples == 0)
    return;

  // 1. Mono-to-Stereo and Standalone Summing
  // We want to ensure that no matter what, we have a signal in both L and R if
  // possible.
  if (wrapperType == juce::AudioProcessor::wrapperType_Standalone) {
    if (totalIns >= 1 && bufferChannels >= 1) {
      // Standalone Failsafe: Scan ALL reported hardware inputs.
      // If we find signal on ANY channel (like channel 3 or 4), sum it to our
      // main pair.
      bool foundAnySignal = false;
      for (int ch = 0; ch < totalIns; ++ch) {
        if (ch < bufferChannels &&
            buffer.getMagnitude(ch, 0, numSamples) > 0.0001f) {
          foundAnySignal = true;
          if (ch > 1 && bufferChannels >= 2) {
            // If signal is on a higher channel, add it to L/R
            buffer.addFrom(0, 0, buffer, ch, 0, numSamples, 0.5f);
            buffer.addFrom(1, 0, buffer, ch, 0, numSamples, 0.5f);
          }
        }
      }

      float lMag = buffer.getMagnitude(0, 0, numSamples);
      float rMag =
          (bufferChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : 0.0f;

      // If we only have signal on one side, copy it to the other
      if (lMag < 0.001f && rMag > 0.001f && bufferChannels >= 2) {
        buffer.copyFrom(0, 0, buffer, 1, 0, numSamples);
      } else if (rMag < 0.001f && lMag > 0.001f && bufferChannels >= 2) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
      }
      // If we only have one input channel total, copy it to the second buffer
      // channel
      else if (totalIns == 1 && bufferChannels >= 2 && lMag > 0.001f) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
      }
    }
  } else {
    // Normal Plugin Behavior (VST3/AU)
    if (totalIns == 1 && bufferChannels >= 2) {
      buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
  }

  // 2. Clear ONLY truly unused channels
  for (int ch = std::max(totalIns, 2); ch < bufferChannels; ++ch) {
    buffer.clear(ch, 0, numSamples);
  }

  const bool tunerOn = (apvts.getRawParameterValue("tunerOn")->load() > 0.5f);
  tunerIsOn.store(tunerOn, std::memory_order_relaxed);

  // Tuner Tap (Pre-Gate/Amp)
  if (tunerOn) {
    const int n = buffer.getNumSamples();
    jassert(n <= tunerScratchMono.getNumSamples());
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

  // HARD DSP BYPASS OR MASTER OFF
  if (bypassParam->load() > 0.5f ||
      (masterOnParam != nullptr && masterOnParam->load() < 0.5f)) {
    return;
  }

  // ===== UPDATE PARAMETERS =====

  // 1. Octaver / Env
  {
    auto &octEnv = dspChain.getOctEnv();
    const bool octOn = octOnParam->load() > 0.5f;
    const bool envOn = envOnParam->load() > 0.5f;

    const bool anyActive = octOn || envOn;
    dspChain.setBypassedAndReset<3>(!anyActive);

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
    dspChain.setBypassed<1>(!gateOn);
    dspChain.getSmartGate().setEnabled(gateOn);
  }

  // 2. Input Gain (Drive) + Comp Input
  {
    float compInDb = compInputParam->load();
    dspChain.getInputGain().setGainDecibels(compInDb);
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
    dspChain.setBypassed<4>(!lcOn);

    // Auto-Gain
    amp.setAutoGain(ampAutoGainParam->load() > 0.5f);

    // Amp Gain and Volume
    amp.setInputGainDb(ampGainParam->load());
    amp.setVolumeDb(ampVolumeParam->load());
  }

  // 4. Compressor
  {
    auto &comp = dspChain.getCompressor();
    const bool compOn = compOnParam->load() > 0.5f;

    dspChain.setBypassedAndReset<2>(!compOn);

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
    dspChain.setBypassedAndReset<7>(!anyModActive);

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

    dspChain.setBypassedAndReset<6>(!mojoTargetActive);

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
    bool autoG = autoGainParam->load() > 0.5f;
    if (autoG != lastAutoGainState) {
      out.resetAutoGainComp(); // Reset to 1.0 on toggle
      lastAutoGainState = autoG;
    }
    out.setAutoGain(autoG);
    out.setGainDecibels(masterOutParam->load());
    out.setSafetyClipThreshold(0.99f);

    out.setMonoMakerFreq(monoMakerParam->load());
    out.setMonoMakerEnabled(monoMakerOnParam->load() > 0.5f);
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
    const double totalAvailable =
        buffer.getNumSamples() /
        (getSampleRate() > 0 ? getSampleRate() : 44100.0);

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
  if (mixVal < 0.999f &&
      dryBuffer.getNumChannels() >= buffer.getNumChannels()) {
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

  if (presetName == "Bootsy`s Cat") {
    setVal("ampBass", 0.0f);
    setVal("ampGain", 8.40036678314209f);
    setVal("ampMid", 0.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 0.0f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 34.79848480224609f);
    setVal("chRate", 0.04278929904103279f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 103.6149368286133f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 1.0f);
    setVal("envDecay", 0.6140377521514893f);
    setVal("envOn", 1.0f);
    setVal("envRange", 62.45429992675781f);
    setVal("masterOut", 2.52780818939209f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 17.35837554931641f);
    setVal("oct2", 61.60457611083984f);
    setVal("octMix", 5.146412372589111f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 50.0f);
    setVal("phRate", 0.07829456031322479f);
    setVal("phaserOn", 1.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Doom Moose") {
    setVal("ampBass", 7.999999523162842f);
    setVal("ampGain", 24.0f);
    setVal("ampMid", 3.999999761581421f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", -1.99999988079071f);
    setVal("ampVolume", -3.210526943206787f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -10.0f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 70.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 23.38652420043945f);
    setVal("octModern", 1.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 90.07537078857422f);
    setVal("phRate", 0.04015733301639557f);
    setVal("phaserOn", 1.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Flea`s Darkglass Unit") {
    setVal("ampBass", 6.000000953674316f);
    setVal("ampGain", 20.0f);
    setVal("ampMid", 2.000000953674316f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 9.0f);
    setVal("ampVolume", -6.54304838180542f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -6.999999523162842f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 0.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Geddy`s Roar") {
    setVal("ampBass", 2.999999284744263f);
    setVal("ampGain", 24.0f);
    setVal("ampMid", 6.999999046325684f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 6.999999046325684f);
    setVal("ampVolume", 4.999999046325684f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 0.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -18.36061859130859f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
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
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 0.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Jaco`s Delight") {
    setVal("ampBass", -3.000000476837158f);
    setVal("ampGain", 4.523303031921387f);
    setVal("ampMid", 1.92146372795105f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 2.000000953674316f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 7.03499174118042f);
    setVal("compInput", -5.66424036026001f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 1.0f);
    setVal("compRelease", 106.3079605102539f);
    setVal("compThresh", -22.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 3.829372644424438f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Jamerson`s Cup") {
    setVal("ampBass", 6.851078510284424f);
    setVal("ampGain", 0.9376201629638672f);
    setVal("ampMid", 2.000000953674316f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", -15.56235599517822f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -14.00000095367432f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 2.999999046325684f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
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
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Level 42 Slap") {
    setVal("ampBass", 5.0f);
    setVal("ampGain", 9.999999046325684f);
    setVal("ampMid", -5.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 11.99999904632568f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 14.69113826751709f);
    setVal("chRate", 0.06960643827915192f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 1.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 3.0f);
    setVal("compRelease", 107.9114761352539f);
    setVal("compThresh", -21.7010555267334f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 0.4217716455459595f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Miller Slap ita!") {
    setVal("ampBass", 6.999999046325684f);
    setVal("ampGain", 8.000000953674316f);
    setVal("ampMid", -7.999999523162842f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 14.84849452972412f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 2.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -19.99999809265137f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -1.000001192092896f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 1.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 82.07083129882812f);
    return;
  }

  if (presetName == "Motown 15\"") {
    setVal("ampBass", 13.34184455871582f);
    setVal("ampGain", 3.0f);
    setVal("ampMid", 2.000000953674316f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", -17.76222610473633f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -10.00000095367432f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -0.01274406909942627f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
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
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Palladino`s P-Bass Vibe") {
    setVal("ampBass", 6.980142116546631f);
    setVal("ampGain", 6.0f);
    setVal("ampMid", 7.102592468261719f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", -3.000000476837158f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.3499999940395355f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 50.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 250.0f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 1.99999988079071f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.550000011920929f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.449999988079071f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 2.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Sledge the Hammer") {
    setVal("ampBass", 2.999999284744263f);
    setVal("ampGain", 8.597637176513672f);
    setVal("ampMid", 5.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 6.000000953674316f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 38.06496810913086f);
    setVal("chRate", 0.0782184973359108f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -26.01983833312988f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 2.741891384124756f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 0.0f);
    setVal("octMix", 33.1697998046875f);
    setVal("octModern", 1.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 0.2624113857746124f);
    setVal("phMix", 18.39975547790527f);
    setVal("phRate", 0.03673106804490089f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 0.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 330.1504211425781f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 0.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Synth Dreams") {
    setVal("ampBass", 2.999999284744263f);
    setVal("ampGain", 18.97550201416016f);
    setVal("ampMid", -0.6247744560241699f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 13.4511833190918f);
    setVal("ampVolume", -0.241225004196167f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 56.22258758544922f);
    setVal("chRate", 0.0782184973359108f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 16.71460914611816f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 2.0f);
    setVal("compRelease", 235.52783203125f);
    setVal("compThresh", -26.01983833312988f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 1.0f);
    setVal("envRange", 56.9786491394043f);
    setVal("masterOut", -1.364867091178894f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 42.58097839355469f);
    setVal("oct2", 38.40481948852539f);
    setVal("octMix", 26.33305740356445f);
    setVal("octModern", 1.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 0.3621650338172913f);
    setVal("phMix", 23.26067161560059f);
    setVal("phRate", 0.02098705992102623f);
    setVal("phaserOn", 1.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 1.0f);
    setVal("cabType", 1.0f);
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 1.0f);
    setVal("monoMaker", 330.1504211425781f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Try to be Guitarish") {
    setVal("ampBass", 2.999999284744263f);
    setVal("ampGain", 8.597637176513672f);
    setVal("ampMid", 5.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 6.000000953674316f);
    setVal("ampVolume", -0.241225004196167f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 38.06496810913086f);
    setVal("chRate", 0.0782184973359108f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -26.01983833312988f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 2.741891384124756f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 0.0f);
    setVal("oct2", 82.36775970458984f);
    setVal("octMix", 18.17819976806641f);
    setVal("octModern", 1.0f);
    setVal("octOn", 1.0f);
    setVal("phColour", 1.0f);
    setVal("phMix", 23.26067161560059f);
    setVal("phRate", 1.0f);
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
    setVal("monoMaker", 330.1504211425781f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Walkin`on the Moon") {
    setVal("ampBass", 2.999999284744263f);
    setVal("ampGain", 9.999999046325684f);
    setVal("ampMid", 5.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 6.000000953674316f);
    setVal("ampVolume", 3.576278686523438e-07f);
    setVal("chDepth", 0.550000011920929f);
    setVal("chMix", 38.06496810913086f);
    setVal("chRate", 0.0782184973359108f);
    setVal("chorusOn", 1.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -22.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.3499999940395355f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", -1.000001192092896f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.2624113857746124f);
    setVal("phMix", 18.39975547790527f);
    setVal("phRate", 0.03673106804490089f);
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
    setVal("monoMaker", 400.0f);
    setVal("lowCutOn", 1.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 0.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  if (presetName == "Default") {
    setVal("ampBass", 0.0f);
    setVal("ampGain", -6.0f);
    setVal("ampMid", 0.0f);
    setVal("ampOn", 1.0f);
    setVal("ampTreble", 0.0f);
    setVal("ampVolume", 3.0f);
    setVal("chDepth", 0.55f);
    setVal("chMix", 0.0f);
    setVal("chRate", 0.35f);
    setVal("chorusOn", 0.0f);
    setVal("compAttack", 10.0f);
    setVal("compInput", 0.0f);
    setVal("compMakeup", 2.0f);
    setVal("compOn", 1.0f);
    setVal("compRatio", 0.0f);
    setVal("compRelease", 120.0f);
    setVal("compThresh", -18.0f);
    setVal("envAttack", 0.25f);
    setVal("envDecay", 0.35f);
    setVal("envOn", 0.0f);
    setVal("envRange", 0.0f);
    setVal("masterOut", 0.0f);
    setVal("masterOn", 1.0f);
    setVal("oct1", 40.0f);
    setVal("oct2", 40.0f);
    setVal("octMix", 0.0f);
    setVal("octModern", 1.0f);
    setVal("octOn", 0.0f);
    setVal("phColour", 0.55f);
    setVal("phMix", 0.0f);
    setVal("phRate", 0.45f);
    setVal("phaserOn", 0.0f);
    setVal("slap", 0.0f);
    setVal("skin", 0.0f);
    setVal("punch", 1.0f);
    setVal("tubeOn", 0.0f);
    setVal("cabType", 1.0f); // 4x10 ON by default
    setVal("masterMix", 100.0f);
    setVal("bypass", 0.0f);
    setVal("autoGain", 1.0f);
    setVal("fxParallel", 0.0f);
    setVal("monoMaker", 20.0f);
    setVal("lowCutOn", 0.0f);
    setVal("monoMakerOn", 1.0f);
    setVal("ampAutoGain", 1.0f);
    setVal("compAutoMakeup", 1.0f);
    setVal("autoGate", 1.0f);
    setVal("tunerOn", 0.0f);
    setVal("gateHoldMs", 90.0f);
    setVal("gateThresh", -66.30000305175781f);
    setVal("irMix", 100.0f);
    return;
  }

  auto file = getPresetsFolder().getChildFile(presetName + ".xml");
  if (file.existsAsFile()) {
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
      apvts.replaceState(juce::ValueTree::fromXml(*xml));
  }
}

juce::StringArray FunkyMooseAudioProcessor::getPresetList() {
  juce::StringArray list;
  list.add("F:Default");
  list.add("F:Bootsy`s Cat");
  list.add("F:Doom Moose");
  list.add("F:Flea`s Darkglass Unit");
  list.add("F:Geddy`s Roar");
  list.add("F:Jaco`s Delight");
  list.add("F:Jamerson`s Cup");
  list.add("F:Level 42 Slap");
  list.add("F:Miller Slap ita!");
  list.add("F:Motown 15\"");
  list.add("F:Palladino`s P-Bass Vibe");
  list.add("F:Sledge the Hammer");
  list.add("F:Synth Dreams");
  list.add("F:Try to be Guitarish");
  list.add("F:Walkin`on the Moon");

  auto folder = getPresetsFolder();
  juce::Array<juce::File> files;
  folder.findChildFiles(files, juce::File::findFiles, false, "*.xml");
  for (auto &f : files)
    list.add("U:" + f.getFileNameWithoutExtension());
  return list;
}

void FunkyMooseAudioProcessor::savePreset(const juce::String &presetName) {
  auto file = getPresetsFolder().getChildFile(presetName + ".xml");
  auto state = apvts.copyState();
  state.setProperty("version", projectVersion, nullptr);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  xml->writeTo(file);
}

void FunkyMooseAudioProcessor::loadFactoryPresets() {}

//==============================================================================
// MIDI LEARN Custom Implementation
//==============================================================================

int FunkyMooseAudioProcessor::findParameterIndexByID(
    const juce::String &parameterID) const {
  // Linear search is fine (small parameter count) and allocation-free.
  for (size_t i = 0; i < parameterIDs.size(); ++i)
    if (parameterIDs[i] == parameterID)
      return (int)i;

  return -1;
}

void FunkyMooseAudioProcessor::handleAsyncUpdate() {
  if (!midiMapDirty.exchange(false))
    return;

  saveMidiMap();
}

void FunkyMooseAudioProcessor::parameterChanged(const juce::String &parameterID,
                                                float newValue) {
  juce::ignoreUnused(newValue);

  // MIDI Learn: user arms learn in UI, then moves a parameter.
  // This callback must be allocation-free and lock-free.
  if (isMidiLearnActive.exchange(false)) {
    const int idx = findParameterIndexByID(parameterID);
    learningParamIndex.store(idx);
  }
}

void FunkyMooseAudioProcessor::clearMidiMapping(const juce::String &paramID) {
  const int idx = findParameterIndexByID(paramID);
  if (idx < 0)
    return;

  for (auto &v : ccToParamIndex)
    if (v.load() == idx)
      v.store(-1);

  midiMapDirty.store(true);
  triggerAsyncUpdate();
}

void FunkyMooseAudioProcessor::saveMidiMap() {
  auto folder = getPresetsFolder().getParentDirectory();
  auto mapFile = folder.getChildFile("midi_map.xml");

  juce::XmlElement xml("MidiMap");

  for (int cc = 0; cc < 128; ++cc) {
    const int idx = ccToParamIndex[(size_t)cc].load();
    if (idx >= 0 && idx < (int)parameterIDs.size()) {
      auto *mapChild = xml.createNewChildElement("Map");
      mapChild->setAttribute("cc", cc);
      mapChild->setAttribute("param", parameterIDs[(size_t)idx]);
    }
  }

  xml.writeTo(mapFile);
}

void FunkyMooseAudioProcessor::loadMidiMap() {
  auto folder = getPresetsFolder().getParentDirectory();
  auto mapFile = folder.getChildFile("midi_map.xml");
  if (!mapFile.existsAsFile())
    return;

  for (auto &v : ccToParamIndex)
    v.store(-1);

  if (auto xml = juce::parseXML(mapFile)) {
    if (xml->hasTagName("MidiMap")) {
      for (auto *child : xml->getChildIterator()) {
        if (child->hasTagName("Map")) {
          const int cc = child->getIntAttribute("cc", -1);
          const juce::String param = child->getStringAttribute("param");
          const int idx = findParameterIndexByID(param);

          if (cc >= 0 && cc < 128 && idx >= 0)
            ccToParamIndex[(size_t)cc].store(idx);
        }
      }
    }
  }
}
