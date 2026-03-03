#pragma once
#include "JuceIncludes.h"
#include "dsp/AudioFifo.h"
#include "dsp/MooseDSPChain.h"
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
class FunkyMooseAudioProcessor : public juce::AudioProcessor {
public:
  static constexpr int projectVersion = 101;

  FunkyMooseAudioProcessor();
  ~FunkyMooseAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override {
    return "FUNKY MOOSE BASS STATEGY";
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

  std::atomic<float> rmsLinear{0.0f};

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

  std::atomic<float> cpuUsage{0.0f};

  AudioFifo tunerFifo;
  juce::AudioBuffer<float> tunerScratchMono;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
      tunerMuteSmoothed;
  std::atomic<bool> tunerIsOn{false};

  MooseDSPChain dspChain;

  // Cached parameters for high-performance retrieval
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
  std::atomic<float> *monoMakerParam = nullptr;
  std::atomic<float> *monoMakerOnParam = nullptr;
  std::atomic<float> *autoGainParam = nullptr;
  std::atomic<float> *forceMonoInputParam = nullptr;

private:
  juce::AudioBuffer<float> dryBuffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseAudioProcessor)
};
