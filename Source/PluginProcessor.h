#pragma once
#include "JuceIncludes.h"
#include "dsp/AudioFifo.h"
#include "dsp/MooseDSPChain.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
class FunkyMooseAudioProcessor
    : public juce::AudioProcessor,
      public juce::AudioProcessorValueTreeState::Listener,
      private juce::AsyncUpdater {
public:
  static constexpr int projectVersion = 130;

  FunkyMooseAudioProcessor();
  ~FunkyMooseAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override {
    return "Funky Moose Amp";
  }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock &) override;
  void setStateInformation(const void *, int) override;

  // Preset management
  juce::StringArray getPresetList();
  void loadPreset(const juce::String &presetName);
  void savePreset(const juce::String &presetName);
  juce::File getPresetsFolder();
  void loadFactoryPresets();
  juce::String currentPresetName;

  std::atomic<float> rmsLinear{0.0f};

  // MIDI Learn (realtime-safe)
  // UI flow stays the same: user enables learn, then moves a parameter -> next
  // CC maps to it.
  std::atomic<bool> isMidiLearnActive{false};

  // Index of the parameter we are currently learning (set in parameterChanged
  // when learn is armed)
  std::atomic<int> learningParamIndex{-1};

  // Set by audio thread when a CC is captured during learn; consumed on message
  // thread for persistence
  std::atomic<int> learnedCC{-1};

  // Marks that mapping changed; triggers AsyncUpdater to save on message thread
  std::atomic<bool> midiMapDirty{false};
  std::atomic<bool> latencyDirty{false};

  // O(1) CC->Param mapping: -1 means unmapped (Realtime-Safe)
  std::atomic<int> ccToParamIndex[128];

  // Parameter registry (stable for life of processor)
  std::vector<juce::RangedAudioParameter *> parameters;
  std::vector<juce::String> parameterIDs;

  void clearMidiMapping(const juce::String &paramID);
  void saveMidiMap();
  void loadMidiMap();

  void updateLatency();

  void handleAkaiMpkMiniMapping(int ccNum, float ccVal);
  void handleAkaiMpkMiniToggles(const juce::MidiMessage &msg, int noteNum);

  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;

  juce::AudioProcessorValueTreeState apvts;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

  // ===== UI meter taps (thread-safe from DSP chain) =====
  float getInRms() const noexcept {
    return dspChain.getOutputGain().getInRms();
  }
  float getOutRms() const noexcept {
    return dspChain.getOutputGain().getOutRms();
  }
  float getCPUUsage() const noexcept { return cpuUsage.load(); }
  float getMidiActivity() const noexcept { return midiActivity.load(); }
  float getCompGainReductionDb() const noexcept {
    return dspChain.getCompressor().getGainReductionDb();
  }
  float getSaturationLevel() const noexcept {
    return dspChain.getAmpTone().getSaturationLevel();
  }
  float getGateActivity() const noexcept {
    return dspChain.getSmartGate().getGateActivity();
  }
  // Punch enabled state for UI handled via parameter listener or polling?
  // The UI queries this via parameter usually. "Punch" button state is enough?
  // Or isPunchEnabledForUI used for animation?
  bool isPunchEnabledForUI() const noexcept { return punchEnabledForUI.load(); }

  AudioFifo &getTunerFifo() { return tunerFifo; }
  std::atomic<bool> &getTunerFlag() { return tunerIsOn; }

  // internal DSP state structure
  struct DSPState {
    juce::dsp::ProcessSpec spec{44100.0, 512, 2};
    bool prepared = false;
  };

  DSPState mState;

  // Exposed to editor for Elch animation.
  std::atomic<bool> punchEnabledForUI{false};

  bool lastGateOn = true;
  bool lastAmpOn = true;
  bool lastCompOn = false;
  bool lastOctOn = false;
  bool lastEnvOn = false;
  bool lastPhaserOn = false;
  bool lastChorusOn = false;
  bool lastMojoOn = true;
  bool lastLowCutOn = false;
  bool lastMonoMakerOn = true;
  bool lastAutoGainState = false;
  bool firstProcessBlockCall = true;

  std::atomic<float> cpuUsage{0.0f};
  std::atomic<float> midiActivity{0.0f};
  std::atomic<int> lastMidiType{0}; // 1: Note, 2: CC
  std::atomic<int> lastMidiNumber{-1};

  AudioFifo tunerFifo;
  juce::AudioBuffer<float> tunerScratchMono;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
      tunerMuteSmoothed;
  std::atomic<bool> tunerIsOn{false};

  MooseDSPChain dspChain;

  // Cached parameters for high-performance retrieval
  // autoGateParam = SmartGate on/off, ampAutoGainParam = amp auto-gain,
  // autoGainParam = output auto-gain
  std::atomic<float> *bypassParam = nullptr;
  std::atomic<float> *autoGateParam = nullptr;
  std::atomic<float> *octOnParam = nullptr;
  std::atomic<float> *oct1Param = nullptr;
  std::atomic<float> *oct2Param = nullptr;
  std::atomic<float> *octMixParam = nullptr;
  std::atomic<float> *envOnParam = nullptr;
  std::atomic<float> *envDecayParam = nullptr;
  std::atomic<float> *envRangeParam = nullptr;
  std::atomic<float> *ampOnParam = nullptr;
  std::atomic<float> *tubeOnParam = nullptr;
  std::atomic<float> *slapParam = nullptr;
  std::atomic<float> *ampBassParam = nullptr;
  std::atomic<float> *ampMidParam = nullptr;
  std::atomic<float> *ampTrebleParam = nullptr;
  std::atomic<float> *ampVolumeParam = nullptr;
  std::atomic<float> *ampGainParam = nullptr;
  std::atomic<float> *ampAutoGainParam = nullptr;
  std::atomic<float> *lowCutOnParam = nullptr;
  std::atomic<float> *compOnParam = nullptr;
  std::atomic<float> *compInputParam = nullptr;
  std::atomic<float> *compThreshParam = nullptr;
  std::atomic<float> *compMakeupParam = nullptr;
  std::atomic<float> *compRatioParam = nullptr;
  std::atomic<float> *compAttackParam = nullptr;
  std::atomic<float> *compReleaseParam = nullptr;
  std::atomic<float> *compMixParam = nullptr;
  std::atomic<float> *compAutoMakeupParam = nullptr;
  std::atomic<float> *punchParam = nullptr;
  std::atomic<float> *phaserOnParam = nullptr;
  std::atomic<float> *phRateParam = nullptr;
  std::atomic<float> *phMixParam = nullptr;
  std::atomic<float> *phColourParam = nullptr;
  std::atomic<float> *chorusOnParam = nullptr;
  std::atomic<float> *chRateParam = nullptr;
  std::atomic<float> *chDepthParam = nullptr;
  std::atomic<float> *chMixParam = nullptr;
  std::atomic<float> *fxParallelParam = nullptr;
  std::atomic<float> *cabTypeParam = nullptr;
  std::atomic<float> *irMixParam = nullptr;
  std::atomic<float> *masterOutParam = nullptr;
  std::atomic<float> *masterMixParam = nullptr;
  std::atomic<float> *masterOnParam = nullptr;
  std::atomic<float> *monoMakerParam = nullptr;
  std::atomic<float> *monoMakerOnParam = nullptr;
  std::atomic<float> *autoGainParam = nullptr;
  std::atomic<float> *forceMonoInputParam = nullptr;
  std::atomic<float> *tunerOnParam = nullptr;
  std::atomic<float> *mojoCrossoverParam = nullptr;
  std::atomic<float> *mojoClankParam = nullptr;
  std::atomic<float> *ampSagParam = nullptr;
  std::atomic<float> *outThicknessParam = nullptr;

private:
  void handleAsyncUpdate() override;

  int findParameterIndexByID(const juce::String &parameterID) const;
  int findParameterIndexByID(const char *parameterID) const;
  void queueMidiParameterChange(int parameterIndex, float normalisedValue);
  void flushQueuedMidiParameterChanges();

  static constexpr size_t maxQueuedMidiParameters = 128;
  std::array<std::atomic<float>, maxQueuedMidiParameters> queuedMidiValues{};
  std::array<std::atomic<bool>, maxQueuedMidiParameters> queuedMidiDirty{};
  std::atomic<bool> queuedMidiChangesPending{false};

  juce::AudioBuffer<float> dryBuffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseAudioProcessor)
};
