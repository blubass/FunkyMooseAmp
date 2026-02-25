#include "PluginProcessor.h"

// JUCE plugin entry point (needed for Standalone; also fine for AU/VST3)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FunkyMooseAudioProcessor();
}