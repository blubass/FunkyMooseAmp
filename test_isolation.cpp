#include <iostream>
#include <juce_audio_devices/juce_audio_devices.h>

int main() {
    juce::AudioDeviceManager deviceManager;
    deviceManager.initialiseWithDefaultDevices(2, 2);
    
    auto inputDev = deviceManager.getCurrentAudioDevice();
    if (inputDev) {
        std::cout << "Input Device: " << inputDev->getName().toStdString() << std::endl;
        std::cout << "Input Channels: " << inputDev->getActiveInputChannels().countNumberOfSetBits() << std::endl;
        std::cout << "Output Channels: " << inputDev->getActiveOutputChannels().countNumberOfSetBits() << std::endl;
    } else {
        std::cout << "ERROR: No audio device found!" << std::endl;
    }
    
    return 0;
}
