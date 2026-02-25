#pragma once
#include "../JuceIncludes.h"

class AudioFifo {
public:
  void prepare(int channels, int capacitySamples) {
    fifo.setTotalSize(capacitySamples);
    buffer.setSize(channels, capacitySamples);
    buffer.clear();
  }

  void push(const float *data, int numSamples) // audio thread
  {
    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    if (size1 > 0)
      buffer.copyFrom(0, start1, data, size1);
    if (size2 > 0)
      buffer.copyFrom(0, start2, data + size1, size2);

    fifo.finishedWrite(size1 + size2);
  }

  int pull(float *dest, int maxSamples) // gui thread
  {
    int start1, size1, start2, size2;
    fifo.prepareToRead(maxSamples, start1, size1, start2, size2);

    if (size1 > 0)
      juce::FloatVectorOperations::copy(dest, buffer.getReadPointer(0, start1),
                                        size1);
    if (size2 > 0)
      juce::FloatVectorOperations::copy(
          dest + size1, buffer.getReadPointer(0, start2), size2);

    fifo.finishedRead(size1 + size2);
    return size1 + size2;
  }

  int getNumReady() const { return fifo.getNumReady(); }

private:
  juce::AbstractFifo fifo{1};
  juce::AudioBuffer<float> buffer;
};
