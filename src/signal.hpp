#pragma once

#include <cstdint>
#include <vector>

#include "qnm.hpp"

namespace kerr {

struct RingdownParams {
    double massSolar = 68.5;  // solar masses
    double spin = 0.69;
    double A0 = 1e-21;
    double inclination = 0.4;  // rad
    double phi0 = 0.0;
    bool noiseEnabled = false;
    double noiseScale = 0.05;
    int overtones = 1;
    bool show33 = false;
    double sampleRate = 4096;
    double duration = 0.04;
    double mergerFrac = 0.2;
    std::uint32_t seed = 1;
};

struct Ringdown {
    std::vector<double> times;
    std::vector<double> strain;
    QNM qnm0, qnm1, qnm33;
    int mergerIdx = 0;
    double dt = 0;
    double sampleRate = 0;
    double duration = 0;
};

// 2 tau before merger, 8 tau after
struct Timing {
    double sampleRate, duration, mergerFrac;
};
Timing chooseTiming(double massSolar, double spin);

// h(t) = sum A_n exp(-t/tau_n) cos(2 pi f_n t + phi_n) after merger
Ringdown generateRingdown(const RingdownParams& p);

// radix-2 fft, size must be power of 2
void fft(std::vector<double>& re, std::vector<double>& im);

struct Spectrum {
    std::vector<double> freqs, amp;
};
Spectrum powerSpectrum(const std::vector<double>& strain, double sampleRate);

struct Spectrogram {
    std::vector<double> tCenter;
    std::vector<std::vector<float>> mags;
    double freqRes = 0;
    int bins = 0;
};
Spectrogram spectrogram(const std::vector<double>& strain, double sampleRate, int winSize = 256,
                        int hopSize = 24);

}  // namespace kerr
