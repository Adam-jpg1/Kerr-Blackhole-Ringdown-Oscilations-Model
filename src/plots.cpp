#include "plots.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "format.hpp"
#include "theme.hpp"

namespace kerr {
namespace {

std::string alpha(const char* hex, const char* aa) { return std::string(hex) + aa; }

void background(Svg& c, double W, double H) {
    c.fillStyle = theme::bg;
    c.fillRect(0, 0, W, H);
}

void font(Svg& c, double px, const char* align) {
    c.fontSize = px;
    c.fontWeight = 400;
    c.textAlign = align;
}

void yAxisTitle(Svg& c, double H, const std::string& title) {
    c.save();
    c.translate(14, H / 2);
    c.rotate(-kPi / 2);
    c.fillText(title, 0, 0);
    c.restore();
}

}  // namespace

// waveform

void drawWaveform(Svg& c, double W, double H, const Ringdown& d, double zoom) {
    background(c, W, H);
    const double PL = 56, PR = 16, PT = 20, PB = 40;
    const double cw = W - PL - PR, ch = H - PT - PB;
    const auto& strain = d.strain;
    const int total = static_cast<int>(strain.size());

    const int viewStart = std::max(0, d.mergerIdx - static_cast<int>(std::floor(total * 0.25 / zoom)));
    const int viewEnd = std::min(total, d.mergerIdx + static_cast<int>(std::floor(total * 0.75 / zoom)));
    const int viewN = viewEnd - viewStart;

    double yMax = 0;
    for (int i = viewStart; i < viewEnd; ++i) yMax = std::max(yMax, std::fabs(strain[i]));
    yMax = yMax * 1.15;
    if (yMax == 0) yMax = 1e-22;

    auto toX = [&](double idx) { return PL + ((idx - viewStart) / viewN) * cw; };
    auto toY = [&](double v) { return PT + ch / 2 - (v / yMax) * (ch / 2); };

    // Grid
    c.strokeStyle = "#0a1820";
    c.lineWidth = 0.5;
    for (int j = 0; j <= 4; ++j) {
        const double y = PT + (j / 4.0) * ch;
        c.beginPath(); c.moveTo(PL, y); c.lineTo(PL + cw, y); c.stroke();
    }
    for (int i = 0; i <= 6; ++i) {
        const double x = PL + (i / 6.0) * cw;
        c.beginPath(); c.moveTo(x, PT); c.lineTo(x, PT + ch); c.stroke();
    }

    // Zero line
    c.strokeStyle = "#0f2030";
    c.lineWidth = 1;
    c.setLineDash({3, 6});
    c.beginPath(); c.moveTo(PL, toY(0)); c.lineTo(PL + cw, toY(0)); c.stroke();
    c.setLineDash({});

    // Merger marker
    if (d.mergerIdx >= viewStart && d.mergerIdx < viewEnd) {
        const double mx = toX(d.mergerIdx);
        c.strokeStyle = "#ffffff10";
        c.lineWidth = 1;
        c.setLineDash({2, 4});
        c.beginPath(); c.moveTo(mx, PT); c.lineTo(mx, PT + ch); c.stroke();
        c.setLineDash({});
        c.fillStyle = theme::white;
        font(c, 12, "center");
        c.fillText("merger", mx, PT + 12);
    }

    // glow pass then main line
    const int step = std::max(1, static_cast<int>(std::floor(viewN / (cw * 1.5))));
    auto tracePath = [&] {
        c.beginPath();
        bool first = true;
        for (int i = viewStart; i < viewEnd; i += step) {
            if (first) { c.moveTo(toX(i), toY(strain[i])); first = false; }
            else c.lineTo(toX(i), toY(strain[i]));
        }
    };
    c.shadowColor = theme::cyan;
    c.shadowBlur = 6;
    c.strokeStyle = alpha(theme::cyan, "40");
    c.lineWidth = 3;
    tracePath();
    c.stroke();

    c.shadowBlur = 0;
    c.strokeStyle = theme::cyan;
    c.lineWidth = 1.2;
    tracePath();
    c.stroke();

    // Axes
    c.fillStyle = theme::white;
    font(c, 12, "center");
    for (int i = 0; i <= 6; ++i) {
        const int idx = viewStart + static_cast<int>(std::floor((i / 6.0) * viewN));
        const double tMs = d.times[std::min(idx, total - 1)] * 1000;
        c.fillText(toFixed(tMs, 1), PL + (i / 6.0) * cw, H - 22);
    }
    c.textAlign = "right";
    const double absMax = std::fabs(yMax) != 0 ? std::fabs(yMax) : 1;
    const int exp10 = static_cast<int>(std::floor(std::log10(absMax)));
    for (int j = 0; j <= 4; ++j) {
        const double v = yMax - (j / 4.0) * 2 * yMax;
        const double y = PT + (j / 4.0) * ch;
        c.fillText(toFixed(v / std::pow(10.0, exp10), 1), PL - 3, y + 3);
    }

    c.fillStyle = theme::white;
    font(c, 12, "center");
    c.fillText("time (ms)", W / 2, H - 5);
    const int exp10Title = static_cast<int>(std::floor(std::log10(yMax != 0 ? yMax : 1)));
    yAxisTitle(c, H, "h(t) ×10^" + std::to_string(exp10Title));
}

// spectrogram

void drawSpectrogram(Svg& c, double W, double H, const Ringdown& d) {
    background(c, W, H);
    const double PL = 56, PR = 16, PT = 16, PB = 40;
    const double cw = W - PL - PR, ch = H - PT - PB;

    const Spectrogram sg = spectrogram(d.strain, d.sampleRate, 256, 24);
    if (sg.mags.empty()) return;

    const double fMin = 20, fMax = std::min(d.sampleRate / 2, 2000.0);
    const double lo = std::log10(fMin), hi = std::log10(fMax);
    auto yFrac = [&](double f) { return 1 - (std::log10(f) - lo) / (hi - lo); };

    double maxMag = 1e-30;
    for (const auto& row : sg.mags)
        for (float m : row) maxMag = std::max(maxMag, static_cast<double>(m));

    const double tMin = sg.tCenter.front() * 1000, tMax = sg.tCenter.back() * 1000;
    const int rows = static_cast<int>(sg.mags.size());
    const double cellW = cw / rows;

    // draw cells into an image instead of thousands of rects
    struct Cell { double y0, y1; int k; };
    std::vector<Cell> cells;
    for (int k = 1; k < sg.bins; ++k) {
        const double f = k * sg.freqRes;
        if (f < fMin || f > fMax) continue;
        const double y0 = PT + yFrac((k + 1) * sg.freqRes) * ch;
        const double y1 = PT + yFrac(f) * ch;
        const double cellH = std::max(1.0, y1 - y0);
        cells.push_back({y0, y0 + cellH + 0.5, k});
    }
    const int scale = 2;
    const int imgW = static_cast<int>(std::ceil(cw * scale)), imgH = static_cast<int>(std::ceil(ch * scale));
    std::vector<int> binForRow(imgH, -1);
    for (int py = 0; py < imgH; ++py) {
        const double y = PT + (py + 0.5) / scale;
        for (auto it = cells.rbegin(); it != cells.rend(); ++it)
            if (y >= it->y0 && y < it->y1) { binForRow[py] = it->k; break; }
    }
    const Rgba bgc = parseColor(theme::bg);
    std::vector<std::uint8_t> img(static_cast<std::size_t>(imgW) * imgH * 4);
    for (int px = 0; px < imgW; ++px) {
        const int ci = std::min(rows - 1, static_cast<int>(std::floor(((px + 0.5) / scale) / cellW)));
        for (int py = 0; py < imgH; ++py) {
            std::uint8_t* p = &img[(static_cast<std::size_t>(py) * imgW + px) * 4];
            const int k = binForRow[py];
            if (k < 0) {
                p[0] = bgc.r; p[1] = bgc.g; p[2] = bgc.b; p[3] = 255;
                continue;
            }
            const double intensity = std::pow(sg.mags[ci][k] / maxMag, 0.35);
            p[0] = 0;
            p[1] = static_cast<std::uint8_t>(std::floor(intensity * 200 * std::min(1.0, intensity * 2)));
            p[2] = static_cast<std::uint8_t>(std::floor(100 + intensity * 155));
            p[3] = 255;
        }
    }
    c.image(pngDataUri(imgW, imgH, img), PL, PT, imgW / static_cast<double>(scale),
            imgH / static_cast<double>(scale));

    // Dominant QNM frequency
    const double f0 = d.qnm0.f_Hz;
    if (f0 > fMin && f0 < fMax) {
        const double yQ = PT + yFrac(f0) * ch;
        c.strokeStyle = alpha(theme::cyan, "90");
        c.lineWidth = 1;
        c.setLineDash({4, 4});
        c.beginPath(); c.moveTo(PL, yQ); c.lineTo(PL + cw, yQ); c.stroke();
        c.setLineDash({});
        c.fillStyle = theme::white;
        font(c, 11, "right");
        c.fillText("f₀=" + toFixed(f0, 1) + "Hz", PL + cw - 2, yQ - 3);
    }

    // Axes
    c.fillStyle = theme::white;
    font(c, 12, "center");
    const double ticks[] = {tMin, (tMin + tMax) / 2, tMax};
    for (int i = 0; i < 3; ++i) c.fillText(toFixed(ticks[i], 1), PL + (i / 2.0) * cw, H - 22);
    c.textAlign = "right";
    for (double f : {fMin, 50.0, 100.0, 200.0, 500.0, 1000.0, fMax}) {
        if (f < fMin || f > fMax) continue;
        c.fillText(jsNumber(f), PL - 3, PT + yFrac(f) * ch + 3);
    }
    c.fillStyle = theme::white;
    font(c, 12, "center");
    c.fillText("time (ms)", W / 2, H - 5);
    yAxisTitle(c, H, "frequency (Hz, log)");
}

// power spectrum

void drawPowerSpectrum(Svg& c, double W, double H, const Ringdown& d) {
    background(c, W, H);
    const double PL = 56, PR = 16, PT = 16, PB = 40;
    const double cw = W - PL - PR, ch = H - PT - PB;
    const Spectrum ps = powerSpectrum(d.strain, d.sampleRate);

    const double fMin = 10, fMax = std::min(d.sampleRate / 2, 2500.0);
    const double fLo = std::log10(fMin), fHi = std::log10(fMax);

    double pMax = 1e-40;
    for (std::size_t k = 0; k < ps.freqs.size(); ++k)
        if (ps.freqs[k] >= fMin && ps.freqs[k] <= fMax) pMax = std::max(pMax, ps.amp[k]);
    const double pMin = pMax * 1e-6;
    const double pLo = std::log10(pMin), pHi = std::log10(pMax);

    auto toX = [&](double f) {
        if (f <= 0) return PL;
        return PL + ((std::log10(std::max(f, fMin)) - fLo) / (fHi - fLo)) * cw;
    };
    auto toY = [&](double p) { return PT + (1 - (std::log10(std::max(p, pMin)) - pLo) / (pHi - pLo)) * ch; };

    // Grid
    c.strokeStyle = "#080f18";
    c.lineWidth = 0.5;
    for (double f : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0}) {
        if (f < fMin || f > fMax) continue;
        c.beginPath(); c.moveTo(toX(f), PT); c.lineTo(toX(f), PT + ch); c.stroke();
    }
    for (int i = 0; i <= 5; ++i) {
        const double y = PT + (i / 5.0) * ch;
        c.beginPath(); c.moveTo(PL, y); c.lineTo(PL + cw, y); c.stroke();
    }

    // Spectrum
    c.shadowColor = alpha(theme::cyan, "60");
    c.shadowBlur = 4;
    c.strokeStyle = theme::cyan;
    c.lineWidth = 1.5;
    c.beginPath();
    bool first = true;
    for (std::size_t k = 0; k < ps.freqs.size(); ++k) {
        if (ps.freqs[k] < fMin || ps.freqs[k] > fMax) continue;
        if (first) { c.moveTo(toX(ps.freqs[k]), toY(ps.amp[k])); first = false; }
        else c.lineTo(toX(ps.freqs[k]), toY(ps.amp[k]));
    }
    c.stroke();
    c.shadowBlur = 0;

    // Predicted QNM frequencies
    struct Marker { const QNM* q; const char* label; const char* color; };
    const Marker markers[] = {{&d.qnm0, "f₀ (2,2,0)", theme::cyan},
                              {&d.qnm1, "f₁ (2,2,1)", theme::amber2},
                              {&d.qnm33, "f₃₃", theme::green}};
    int row = 0;
    for (const Marker& m : markers) {
        const double f = m.q->f_Hz;
        if (f < fMin || f > fMax) continue;
        const double x = toX(f);
        c.strokeStyle = alpha(m.color, "70");
        c.lineWidth = 1;
        c.setLineDash({3, 5});
        c.beginPath(); c.moveTo(x, PT); c.lineTo(x, PT + ch); c.stroke();
        c.setLineDash({});
        c.fillStyle = theme::white;
        font(c, 11, "center");
        // stagger labels so neighbouring modes don't overlap
        const double ly = PT + 12 + (row++ % 2) * 34;
        c.fillText(m.label, x, ly);
        c.fillText(toFixed(f, 1) + " Hz", x, ly + 14);
    }

    // Axes
    c.fillStyle = theme::white;
    font(c, 12, "center");
    for (double f : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0}) {
        if (f < fMin || f > fMax) continue;
        c.fillText(jsNumber(f), toX(f), H - 22);
    }
    c.fillText("frequency (Hz)", W / 2, H - 5);
    yAxisTitle(c, H, "|H(f)|");
}

// kerr parameter diagram

void drawKerrDiagram(Svg& c, double W, double H, double massSolar, double spin) {
    background(c, W, H);
    const double PL = 56, PR = 44, PT = 24, PB = 40;
    const double cw = W - PL - PR, ch = H - PT - PB;

    const double masses[] = {10, 30, 68.5, 100, 200};
    const char* colors[] = {"#1a3a55", "#1a4060", "#007a8f", "#009ab0", theme::cyan};

    double fMaxAll = 0;
    for (double m : masses) fMaxAll = std::max(fMaxAll, kerrQNM(m, 0.98, 0).f_Hz);
    const double fMinAll = kerrQNM(masses[4], 0, 0).f_Hz * 0.8;

    auto toX = [&](double a) { return PL + (a / 0.98) * cw; };
    auto toY = [&](double f) { return PT + (1 - (f - fMinAll) / (fMaxAll - fMinAll)) * ch; };
    const double spinTicks[] = {0, 0.2, 0.4, 0.6, 0.8, 0.98};

    c.strokeStyle = "#09141e";
    c.lineWidth = 0.5;
    for (double a : spinTicks) {
        c.beginPath(); c.moveTo(toX(a), PT); c.lineTo(toX(a), PT + ch); c.stroke();
    }

    for (int mi = 0; mi < 5; ++mi) {
        c.strokeStyle = colors[mi];
        c.lineWidth = mi == 4 ? 1.5 : 1;
        c.beginPath();
        for (int i = 0; i < 100; ++i) {
            const double a = i * 0.0098;
            const double x = toX(a), y = toY(kerrQNM(masses[mi], a, 0).f_Hz);
            if (i == 0) c.moveTo(x, y); else c.lineTo(x, y);
        }
        c.stroke();
        c.fillStyle = theme::white;
        font(c, 11, "left");
        c.fillText(jsNumber(masses[mi]) + "M☉", PL + cw + 2, toY(kerrQNM(masses[mi], 0.98, 0).f_Hz) + 3);
    }

    const double f = kerrQNM(massSolar, spin, 0).f_Hz;
    const double cx = toX(spin), cy = toY(f);
    c.fillStyle = theme::cyan;
    c.shadowColor = theme::cyan;
    c.shadowBlur = 10;
    c.beginPath(); c.arc(cx, cy, 5); c.fill();
    c.shadowBlur = 0;
    c.strokeStyle = alpha(theme::cyan, "40");
    c.lineWidth = 1;
    c.setLineDash({2, 4});
    c.beginPath(); c.moveTo(PL, cy); c.lineTo(cx, cy); c.stroke();
    c.beginPath(); c.moveTo(cx, PT + ch); c.lineTo(cx, cy); c.stroke();
    c.setLineDash({});
    c.fillStyle = theme::white;
    font(c, 12, "left");
    c.fillText(toFixed(f, 1) + " Hz", cx + 6, cy - 4);

    // Axes and title
    c.fillStyle = theme::white;
    font(c, 12, "center");
    for (double a : spinTicks) c.fillText(toFixed(a, 2), toX(a), H - 22);
    c.textAlign = "right";
    for (double fy : {fMinAll, (fMinAll + fMaxAll) / 2, fMaxAll}) c.fillText(toFixed(fy, 0), PL - 3, toY(fy) + 3);
    c.textAlign = "center";
    c.fillText("dimensionless spin χ", W / 2, H - 5);
    yAxisTitle(c, H, "f_QNM (Hz)");
    c.fillStyle = theme::white;
    font(c, 12, "center");
    c.fillText("QNM frequency vs. spin (l=m=2, n=0)", W / 2, 15);
}

// detector view

void drawDetectorView(Svg& c, double W, double H, const Ringdown& d, double sens) {
    background(c, W, H);
    const double PL = 60, PR = 16, PT = 16, PB = 40;
    const double cw = W - PL - PR, ch = H - PT - PB;
    const double fMin = 10, fMax = 2000;
    const double fLo = std::log10(fMin), fHi = std::log10(fMax);

    const Spectrum ps = powerSpectrum(d.strain, d.sampleRate);
    double sigMax = 1e-40;
    for (std::size_t k = 0; k < ps.freqs.size(); ++k)
        if (ps.freqs[k] >= fMin && ps.freqs[k] <= fMax) sigMax = std::max(sigMax, ps.amp[k]);

    const double pRef = sigMax / sens;
    const double pMin = pRef * 1e-5, pMax = sigMax * 2;
    const double pLo = std::log10(pMin), pHi = std::log10(pMax);
    auto toX = [&](double f) { return PL + ((std::log10(std::max(f, fMin)) - fLo) / (fHi - fLo)) * cw; };
    auto toY = [&](double p) { return PT + (1 - (std::log10(std::max(p, pMin)) - pLo) / (pHi - pLo)) * ch; };

    c.strokeStyle = "#07111a";
    c.lineWidth = 0.5;
    for (double f : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0}) {
        c.beginPath(); c.moveTo(toX(f), PT); c.lineTo(toX(f), PT + ch); c.stroke();
        c.fillStyle = theme::white;
        font(c, 11, "center");
        c.fillText(jsNumber(f), toX(f), H - 22);
    }

    // rough aLIGO noise curve
    c.strokeStyle = alpha(theme::amber, "50");
    c.lineWidth = 1.5;
    c.setLineDash({4, 6});
    c.beginPath();
    bool first = true;
    for (double fi = fMin; fi <= fMax; fi += 2) {
        const double seismic = pRef * 1e3 * std::pow(fi / 30, -4.5);
        const double thermal = pRef * 0.3 * std::pow(fi / 150, -0.5);
        const double shot = pRef * 0.1 * std::pow(fi / 150, 1.8);
        const double n = std::sqrt(seismic * seismic + thermal * thermal + shot * shot);
        if (first) { c.moveTo(toX(fi), toY(n)); first = false; }
        else c.lineTo(toX(fi), toY(n));
    }
    c.stroke();
    c.setLineDash({});

    c.shadowColor = alpha(theme::cyan, "50");
    c.shadowBlur = 4;
    c.strokeStyle = theme::cyan;
    c.lineWidth = 1.5;
    c.beginPath();
    first = true;
    for (std::size_t k = 0; k < ps.freqs.size(); ++k) {
        if (ps.freqs[k] < fMin || ps.freqs[k] > fMax) continue;
        if (first) { c.moveTo(toX(ps.freqs[k]), toY(ps.amp[k])); first = false; }
        else c.lineTo(toX(ps.freqs[k]), toY(ps.amp[k]));
    }
    c.stroke();
    c.shadowBlur = 0;

    c.lineWidth = 1.5;
    c.strokeStyle = theme::cyan;
    c.beginPath(); c.moveTo(PL + 8, PT + 14); c.lineTo(PL + 28, PT + 14); c.stroke();
    c.strokeStyle = alpha(theme::amber, "90");
    c.setLineDash({4, 6});
    c.beginPath(); c.moveTo(PL + 8, PT + 32); c.lineTo(PL + 28, PT + 32); c.stroke();
    c.setLineDash({});
    c.fillStyle = theme::white;
    font(c, 12, "left");
    c.fillText("signal", PL + 36, PT + 18);
    c.fillText("aLIGO noise", PL + 36, PT + 36);
    c.fillText("SNR " + toFixed(sens, 1), PL + 8, PT + 56);

    c.fillStyle = theme::white;
    font(c, 12, "center");
    c.fillText("frequency (Hz, log)", W / 2, H - 5);
    yAxisTitle(c, H, "strain amplitude");
}

}  // namespace kerr
