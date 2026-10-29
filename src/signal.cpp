#include "signal.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace kerr {

Timing chooseTiming(double massSolar, double spin) {
    const QNM q = kerrQNM(massSolar, spin, 0);
    const double ringdownDur = std::max(q.tau_s * 8, 0.002);
    const double preDur = std::max(q.tau_s * 2, 0.001);
    const double total = preDur + ringdownDur;
    const double sr =
        std::min(16384.0, std::max(4096.0, std::pow(2.0, std::ceil(std::log2(q.f_Hz * 8)))));
    return {sr, total, preDur / total};
}

Ringdown generateRingdown(const RingdownParams& p) {
    Ringdown r;
    const int N = static_cast<int>(std::floor(p.sampleRate * p.duration));
    const double dt = 1.0 / p.sampleRate;
    const int mergerIdx = static_cast<int>(std::floor(N * p.mergerFrac));

    r.qnm0 = kerrQNM(p.massSolar, p.spin, 0);
    r.qnm1 = kerrQNM(p.massSolar, p.spin, 1);
    r.qnm33 = kerrQNM33(p.massSolar, p.spin);
    const QNM& q0 = r.qnm0;
    const QNM& q1 = r.qnm1;
    const QNM& q33 = r.qnm33;

    const double Fp = 0.5 * (1 + std::pow(std::cos(p.inclination), 2));
    const double Fc = std::cos(p.inclination);

    // noise
    std::vector<double> noise;
    if (p.noiseEnabled) {
        std::mt19937 rng(p.seed);
        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        noise.resize(N);
        double prev = 0;
        for (int i = 0; i < N; ++i) {
            const double w = uniform(rng) - 0.5;
            prev = 0.995 * prev + 0.005 * w * 600;
            noise[i] = prev + w * 0.4;
        }
    }

    r.times.resize(N);
    r.strain.resize(N);
    for (int i = 0; i < N; ++i) {
        r.times[i] = i * dt;
        const double t = (i - mergerIdx) * dt;
        double h = 0;

        if (i < mergerIdx) {
            // inspiral chirp
            const double tauMerge = -t;
            if (tauMerge > 0) {
                const double chi = tauMerge / 0.015;
                const double fInspiral = q0.f_Hz * 0.5 * std::pow(std::max(chi, 0.01), -3.0 / 8);
                const double ampInspiral = std::pow(std::max(chi, 0.01), -1.0 / 4);
                const double phase = -2 * kPi * fInspiral * tauMerge;
                h = p.A0 * std::min(ampInspiral * 0.3, 1.0) * Fp * std::sin(phase);
                h *= std::exp(-tauMerge * 8);
            }
        } else if (t >= 0) {
            // ringdown, l=m=2 n=0
            h += p.A0 * Fp * std::exp(-t / q0.tau_s) * std::cos(2 * kPi * q0.f_Hz * t + p.phi0);
            // n=1 overtone
            if (p.overtones >= 1) {
                h += p.A0 * 0.15 * Fp * std::exp(-t / q1.tau_s) *
                     std::cos(2 * kPi * q1.f_Hz * t + p.phi0 + 0.3);
            }
            // l=m=3 mode
            if (p.show33) {
                h += p.A0 * 0.05 * Fc * std::exp(-t / q33.tau_s) *
                     std::cos(2 * kPi * q33.f_Hz * t + p.phi0 + 1.1);
            }
            // merger
            h += p.A0 * 0.25 * std::exp(-t / (q0.tau_s * 0.3)) * std::sin(4 * kPi * q0.f_Hz * t);
        }

        if (!noise.empty()) h += p.noiseScale * p.A0 * noise[i];
        r.strain[i] = h;
    }

    r.mergerIdx = mergerIdx;
    r.dt = dt;
    r.sampleRate = p.sampleRate;
    r.duration = p.duration;
    return r;
}

void fft(std::vector<double>& re, std::vector<double>& im) {
    const std::size_t N = re.size();
    if (N <= 1) return;
    const std::size_t half = N / 2;
    std::vector<double> reEven(half), imEven(half), reOdd(half), imOdd(half);
    for (std::size_t i = 0; i < half; ++i) {
        reEven[i] = re[2 * i];
        imEven[i] = im[2 * i];
        reOdd[i] = re[2 * i + 1];
        imOdd[i] = im[2 * i + 1];
    }
    fft(reEven, imEven);
    fft(reOdd, imOdd);
    for (std::size_t k = 0; k < half; ++k) {
        const double theta = -2 * kPi * static_cast<double>(k) / static_cast<double>(N);
        const double c = std::cos(theta), s = std::sin(theta);
        const double tRe = c * reOdd[k] - s * imOdd[k];
        const double tIm = s * reOdd[k] + c * imOdd[k];
        re[k] = reEven[k] + tRe;
        im[k] = imEven[k] + tIm;
        re[k + half] = reEven[k] - tRe;
        im[k + half] = imEven[k] - tIm;
    }
}

namespace {
double hann(std::size_t i, std::size_t n) {
    return 0.5 * (1 - std::cos(2 * kPi * static_cast<double>(i) / static_cast<double>(n - 1)));
}
}  // namespace

Spectrum powerSpectrum(const std::vector<double>& strain, double sampleRate) {
    std::size_t N = 1;
    while (N < strain.size()) N <<= 1;
    std::vector<double> re(N, 0.0), im(N, 0.0);
    for (std::size_t i = 0; i < strain.size(); ++i) re[i] = strain[i] * hann(i, strain.size());
    fft(re, im);
    const std::size_t half = N / 2;
    Spectrum s;
    s.freqs.resize(half);
    s.amp.resize(half);
    for (std::size_t k = 0; k < half; ++k) {
        s.freqs[k] = static_cast<double>(k) * sampleRate / static_cast<double>(N);
        s.amp[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]) / static_cast<double>(N);
    }
    return s;
}

Spectrogram spectrogram(const std::vector<double>& strain, double sampleRate, int winSize,
                        int hopSize) {
    Spectrogram sg;
    const std::size_t win = static_cast<std::size_t>(winSize);
    for (std::size_t start = 0; start + win <= strain.size(); start += hopSize) {
        std::vector<double> re(win), im(win, 0.0);
        for (std::size_t i = 0; i < win; ++i) re[i] = strain[start + i] * hann(i, win);
        fft(re, im);
        std::vector<float> mags(win / 2);
        for (std::size_t k = 0; k < win / 2; ++k)
            mags[k] = static_cast<float>(std::sqrt(re[k] * re[k] + im[k] * im[k]));
        sg.tCenter.push_back((static_cast<double>(start) + winSize / 2.0) / sampleRate);
        sg.mags.push_back(std::move(mags));
    }
    sg.freqRes = sampleRate / winSize;
    sg.bins = winSize / 2;
    return sg;
}

}  // namespace kerr
