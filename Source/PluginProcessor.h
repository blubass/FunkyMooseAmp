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
  static constexpr int projectVersion = 1;

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
  bool acceptsMidi() const override { return false; }
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

  // Module bypass state tracking for DSP reset
  bool lastAmpOn = true;
  bool lastCompOn = true;
  bool lastLowCutOn = false;
  bool lastMonoMakerOn = true;

  bool lastOctOn = false;
  bool lastEnvOn = false;
  bool lastPhaserOn = false;
  bool lastChorusOn = false;

  std::atomic<float> cpuUsage{0.0f};

  AudioFifo tunerFifo;
  juce::AudioBuffer<float> tunerScratchMono;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
      tunerMuteSmoothed;
  std::atomic<bool> tunerIsOn{false};

private:
  MooseDSPChain dspChain;
  juce::AudioBuffer<float> dryBuffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseAudioProcessor)
};
