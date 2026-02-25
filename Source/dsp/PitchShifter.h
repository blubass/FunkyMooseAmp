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
    osamp = 4;
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
  }

  void prepare(double sampleRate) {
    this->sampleRate = sampleRate;
    freqPerBin = sampleRate / (double)fftFrameSize;
  }

  void process(float pitchShift, int numSampsToProcess, float *indata,
               float *outdata) {
    int fftFrameSize2 = fftFrameSize / 2;

    for (int i = 0; i < numSampsToProcess; i++) {
      gInFIFO[(size_t)rover] = indata[i];
      outdata[i] = gOutFIFO[(size_t)(rover - inFifoLatency)];
      rover++;

      if (rover >= fftFrameSize) {
        rover = inFifoLatency;

        // Windowing
        std::vector<float> fftWorksp(2 * (size_t)fftFrameSize, 0.0f);
        for (int k = 0; k < fftFrameSize; k++) {
          double window = -0.5 * cos(2.0 * juce::MathConstants<double>::pi *
                                     (double)k / (double)fftFrameSize) +
                          0.5;
          fftWorksp[2 * (size_t)k] = (float)(gInFIFO[(size_t)k] * window);
          fftWorksp[2 * (size_t)k + 1] = 0.0f;
        }

        // Analysis FFT
        smbFft(fftWorksp.data(), fftFrameSize, -1);

        for (int k = 0; k <= fftFrameSize2; k++) {
          double real = fftWorksp[2 * (size_t)k];
          double imag = fftWorksp[2 * (size_t)k + 1];
          double magn = 2.0 * sqrt(real * real + imag * imag);
          double phase = atan2(imag, real);

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
        for (int k = 0; k <= fftFrameSize2; k++) {
          double magn = gSynMagn[(size_t)k];
          double tmp = gSynFreq[(size_t)k];
          tmp -= (double)k * freqPerBin;
          tmp /= freqPerBin;
          tmp = 2.0 * juce::MathConstants<double>::pi * tmp / (double)osamp;
          tmp += (double)k * expct;
          gSumPhase[(size_t)k] += (float)tmp;
          double phase = gSumPhase[(size_t)k];

          fftWorksp[2 * (size_t)k] = (float)(magn * cos(phase));
          fftWorksp[2 * (size_t)k + 1] = (float)(magn * sin(phase));
        }

        for (int k = fftFrameSize + 2; k < 2 * fftFrameSize; k++)
          fftWorksp[(size_t)k] = 0.0f;

        // Synthesis FFT
        smbFft(fftWorksp.data(), fftFrameSize, 1);

        // Windowing and Overlap-Add
        for (int k = 0; k < fftFrameSize; k++) {
          double window = -0.5 * cos(2.0 * juce::MathConstants<double>::pi *
                                     (double)k / (double)fftFrameSize) +
                          0.5;
          gOutputAccum[(size_t)k] +=
              (float)(2.0 * window * fftWorksp[2 * (size_t)k] /
                      (double)(fftFrameSize2 * osamp));
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

private:
  void smbFft(float *fftBuffer, long fftFrameSize, long sign) {
    float wr, wi, arg, *p1, *p2, temp;
    float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
    long i, bitm, j, le, le2, k;

    for (i = 2; i < 2 * fftFrameSize - 2; i += 2) {
      for (bitm = 2, j = 0; bitm < 2 * fftFrameSize; bitm <<= 1) {
        if (i & bitm)
          j++;
        j <<= 1;
      }
      if (i < j) {
        p1 = fftBuffer + i;
        p2 = fftBuffer + j;
        temp = *p1;
        *(p1++) = *p2;
        *(p2++) = temp;
        temp = *p1;
        *p1 = *p2;
        *p2 = temp;
      }
    }
    for (k = 0, le = 2; k < (long)(log(fftFrameSize) / log(2.) + .5); k++) {
      le <<= 1;
      le2 = le >> 1;
      ur = 1.0;
      ui = 0.0;
      arg = (float)(juce::MathConstants<double>::pi / (le2 >> 1));
      wr = cos(arg);
      wi = (float)(sign * sin(arg));
      for (j = 0; j < le2; j += 2) {
        p1r = fftBuffer + j;
        p1i = p1r + 1;
        p2r = p1r + le2;
        p2i = p2r + 1;
        for (i = j; i < 2 * fftFrameSize; i += le) {
          tr = *p2r * ur - *p2i * ui;
          ti = *p2r * ui + *p2i * ur;
          *p2r = *p1r - tr;
          *p2i = *p1i - ti;
          *p1r += tr;
          *p1i += ti;
          p1r += le;
          p1i += le;
          p2r += le;
          p2i += le;
        }
        tr = ur * wr - ui * wi;
        ui = ur * wi + ui * wr;
        ur = tr;
      }
    }
  }

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
      gAnaFreq, gAnaMagn, gSynFreq, gSynMagn;
};
