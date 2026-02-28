#pragma once

#include "../JuceIncludes.h"
#include <vector>

/**
    A JUCE-friendly wrapper for the Stephan M. Bernsee STFT Pitch Shifter.
    This class is thread-safe per-instance (no static FIFOs).
*/
class PitchShifter {
public:
  PitchShifter()
      : fft(10) // 2^10 = 1024
  {
    fftFrameSize = 1024;
    osamp =
        4; // Increased from 2 for cleaner sound & better octave-up remapping
    stepSize = fftFrameSize / osamp;
    freqPerBin = 0.0;
    expct = 2.0 * juce::MathConstants<double>::pi * (double)stepSize /
            (double)fftFrameSize;
    inFifoLatency = fftFrameSize - stepSize;
    rover = inFifoLatency;

    gInFIFO.assign(8192, 0.0f);
    gOutFIFO.assign(8192, 0.0f);
    gLastPhase.assign(8192 / 2 + 1, 0.0f);
    gSumPhase.assign(8192 / 2 + 1, 0.0f);
    gOutputAccum.assign(16384, 0.0f);
    gAnaFreq.assign(8192, 0.0f);
    gAnaMagn.assign(8192, 0.0f);
    gSynFreq.assign(8192, 0.0f);
    gSynMagn.assign(8192, 0.0f);
    fftWorksp.assign(2048, 0.0f);
  }

  void reset() {
    std::fill(gInFIFO.begin(), gInFIFO.end(), 0.0f);
    std::fill(gOutFIFO.begin(), gOutFIFO.end(), 0.0f);
    std::fill(gLastPhase.begin(), gLastPhase.end(), 0.0f);
    std::fill(gSumPhase.begin(), gSumPhase.end(), 0.0f);
    std::fill(gOutputAccum.begin(), gOutputAccum.end(), 0.0f);
    std::fill(gAnaFreq.begin(), gAnaFreq.end(), 0.0f);
    std::fill(gAnaMagn.begin(), gAnaMagn.end(), 0.0f);
    std::fill(gSynFreq.begin(), gSynFreq.end(), 0.0f);
    std::fill(gSynMagn.begin(), gSynMagn.end(), 0.0f);
    std::fill(fftWorksp.begin(), fftWorksp.end(), 0.0f);
    rover = inFifoLatency;
  }

  void prepare(double sampleRate) {
    this->sampleRate = sampleRate;
    freqPerBin = sampleRate / (double)fftFrameSize;
    fftWorksp.assign(2 * (size_t)fftFrameSize, 0.0f);
  }

  void process(float pitchShift, int numSampsToProcess, float *indata,
               float *outdata) {
    if (pitchShift < 0.1f)
      pitchShift = 0.5f; // Safety
    int fftFrameSize2 = fftFrameSize / 2;

    for (int i = 0; i < numSampsToProcess; i++) {
      float sample = indata[i];
      if (!std::isfinite(sample))
        sample = 0.0f;

      gInFIFO[(size_t)rover] = sample;
      outdata[i] = gOutFIFO[(size_t)(rover - inFifoLatency)];
      rover++;

      if (rover >= fftFrameSize) {
        rover = inFifoLatency;

        // Windowing
        std::fill(fftWorksp.begin(), fftWorksp.end(), 0.0f);
        for (int k = 0; k < fftFrameSize; k++) {
          double window = -0.5 * cos(2.0 * juce::MathConstants<double>::pi *
                                     (double)k / (double)fftFrameSize) +
                          0.5;
          fftWorksp[(size_t)k] = (float)(gInFIFO[(size_t)k] * window);
        }

        // Analysis FFT (Real to Complex)
        fft.performRealOnlyForwardTransform(fftWorksp.data());

        // Process complex data
        for (int k = 0; k <= fftFrameSize2; k++) {
          float real = fftWorksp[(size_t)(2 * k)];
          float imag = fftWorksp[(size_t)(2 * k + 1)];

          double magn = 2.0 * sqrt(real * real + imag * imag);
          double phase = atan2(imag, real);

          if (!std::isfinite(magn))
            magn = 0.0;
          if (!std::isfinite(phase))
            phase = 0.0;

          double tmp = phase - gLastPhase[(size_t)k];
          gLastPhase[(size_t)k] = (float)phase;

          tmp -= (double)k * expct;
          int qpd = (int)(tmp / juce::MathConstants<double>::pi);
          if (qpd >= 0)
            qpd += qpd & 1;
          else
            qpd -= qpd & 1;
          tmp -= juce::MathConstants<double>::pi * (double)qpd;
          tmp = (double)osamp * tmp / (2.0 * juce::MathConstants<double>::pi);
          tmp = (double)k * freqPerBin + tmp * freqPerBin;

          gAnaMagn[(size_t)k] = (float)magn;
          gAnaFreq[(size_t)k] = (float)tmp;
        }

        // Processing
        std::fill(gSynMagn.begin(), gSynMagn.end(), 0.0f);
        std::fill(gSynFreq.begin(), gSynFreq.end(), 0.0f);
        for (int k = 0; k <= fftFrameSize2; k++) {
          int index = (int)(k * pitchShift);
          if (index <= fftFrameSize2) {
            gSynMagn[(size_t)index] += gAnaMagn[(size_t)k];
            gSynFreq[(size_t)index] = gAnaFreq[(size_t)k] * pitchShift;
          }
        }

        // Synthesis
        std::fill(fftWorksp.begin(), fftWorksp.end(), 0.0f);
        for (int k = 0; k <= fftFrameSize2; k++) {
          double magn = gSynMagn[(size_t)k];
          double tmp = gSynFreq[(size_t)k];
          tmp -= (double)k * freqPerBin;
          tmp /= freqPerBin;
          tmp = 2.0 * juce::MathConstants<double>::pi * tmp / (double)osamp;
          tmp += (double)k * expct;
          gSumPhase[(size_t)k] += (float)tmp;

          // Phase normalization to prevent overflow/buzzing
          if (gSumPhase[(size_t)k] > 1000.0f)
            gSumPhase[(size_t)k] -= 2000.0f;
          if (gSumPhase[(size_t)k] < -1000.0f)
            gSumPhase[(size_t)k] += 2000.0f;

          double phase = gSumPhase[(size_t)k];

          fftWorksp[(size_t)(2 * k)] = (float)(magn * cos(phase));
          fftWorksp[(size_t)(2 * k + 1)] = (float)(magn * sin(phase));
        }

        // Synthesis FFT (Complex to Real)
        fft.performRealOnlyInverseTransform(fftWorksp.data());

        // Windowing and Overlap-Add
        for (int k = 0; k < fftFrameSize; k++) {
          double window = -0.5 * cos(2.0 * juce::MathConstants<double>::pi *
                                     (double)k / (double)fftFrameSize) +
                          0.5;
          // JUCE Inverse Transform already scales by 1/N.
          // Redundant division here made the signal inaudible.
          float outSample = (float)(2.0 * window * fftWorksp[(size_t)k]);
          if (!std::isfinite(outSample))
            outSample = 0.0f;
          gOutputAccum[(size_t)k] += outSample;
        }
        for (int k = 0; k < stepSize; k++)
          gOutFIFO[(size_t)k] = gOutputAccum[(size_t)k];

        std::memmove(gOutputAccum.data(), gOutputAccum.data() + stepSize,
                     (size_t)fftFrameSize * sizeof(float));
        for (int k = 0; k < inFifoLatency; k++)
          gInFIFO[(size_t)k] = gInFIFO[(size_t)(k + stepSize)];
      }
    }
  }

  int getLatency() const { return inFifoLatency; }

private:
  juce::dsp::FFT fft;
  int fftFrameSize;
  int osamp;
  int stepSize;
  double freqPerBin;
  double expct;
  int inFifoLatency;
  int rover;
  double sampleRate = 44100.0;

  std::vector<float> gInFIFO, gOutFIFO, gLastPhase, gSumPhase, gOutputAccum,
      gAnaFreq, gAnaMagn, gSynFreq, gSynMagn, fftWorksp;
};
