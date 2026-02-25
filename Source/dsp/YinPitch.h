#pragma once
#include "../JuceIncludes.h"
#include <vector>

struct YinPitchResult {
  float frequencyHz = 0.0f;
  float probability = 0.0f;
  bool valid = false;
};

class YinPitch {
public:
  void prepare(double sampleRate, int frameSize) {
    sr = sampleRate;
    N = frameSize;
    diff.assign((size_t)N, 0.0f);
    cmnd.assign((size_t)N, 0.0f);
  }

  YinPitchResult process(const float *x, int numSamples) {
    YinPitchResult r{};
    if (numSamples < N)
      return r;

    std::fill(diff.begin(), diff.end(), 0.0f);
    for (int tau = 1; tau < N; ++tau) {
      double sum = 0.0;
      for (int i = 0; i < N - tau; ++i) {
        const double d = (double)x[i] - (double)x[i + tau];
        sum += d * d;
      }
      diff[(size_t)tau] = (float)sum;
    }

    cmnd[0] = 1.0f;
    double runningSum = 0.0;
    for (int tau = 1; tau < N; ++tau) {
      runningSum += diff[(size_t)tau];
      cmnd[(size_t)tau] =
          (runningSum > 0.0)
              ? (diff[(size_t)tau] * (float)tau / (float)runningSum)
              : 1.0f;
    }

    const float thresh = 0.12f; // Bass-stabil
    int tauEstimate = -1;
    for (int tau = 2; tau < N; ++tau) {
      if (cmnd[(size_t)tau] < thresh) {
        while (tau + 1 < N && cmnd[(size_t)(tau + 1)] < cmnd[(size_t)tau])
          ++tau;
        tauEstimate = tau;
        break;
      }
    }
    if (tauEstimate < 0)
      return r;

    int t0 = juce::jmax(tauEstimate - 1, 1);
    int t1 = tauEstimate;
    int t2 = juce::jmin(tauEstimate + 1, N - 1);

    const float s0 = cmnd[(size_t)t0], s1 = cmnd[(size_t)t1],
                s2 = cmnd[(size_t)t2];
    const float denom = (2.0f * s1 - s2 - s0);
    float betterTau =
        (denom != 0.0f) ? (t1 + (s2 - s0) / (2.0f * denom)) : (float)t1;

    const float freq = (betterTau > 0.0f) ? (float)(sr / betterTau) : 0.0f;

    r.frequencyHz = freq;
    r.probability = juce::jlimit(0.0f, 1.0f, 1.0f - cmnd[(size_t)tauEstimate]);
    r.valid = (freq > 20.0f && freq < 2000.0f && r.probability > 0.6f);
    return r;
  }

private:
  double sr = 44100.0;
  int N = 4096;
  std::vector<float> diff, cmnd;
};
