#include "app.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <vector>

#include "format.hpp"
#include "plots.hpp"
#include "svg.hpp"
#include "theme.hpp"

namespace kerr {

namespace {

struct Preset {
    const char* label;
    double mass, spin, A0, inclination, phi0;
};

const std::vector<std::pair<std::string, Preset>>& presets() {
    static const std::vector<std::pair<std::string, Preset>> p = {
        {"GW150914", {"GW150914", 68.5, 0.69, 1.0e-21, 0.4, 0.0}},
        {"HighSpin", {"High-Spin Kerr", 30.0, 0.95, 6.0e-22, 0.3, 0.5}},
        {"IMBH", {"Intermediate-Mass BH", 500, 0.67, 3.0e-22, 0.6, 0.2}},
        {"SMBH", {"Supermassive BH", 1e6, 0.85, 1.0e-17, 0.5, 1.1}},
    };
    return p;
}

// layout helpers (monospace font, each char is 0.6em wide)

constexpr double kW = 1440, kH = 900;
constexpr double kPlotTop = 114, kPlotH = kH - 44 - 29 - 41 - 19, kFooterTop = kPlotTop + kPlotH;

std::string collapseSpaces(const std::string& s) {
    std::string out;
    for (char ch : s)
        if (!(ch == ' ' && !out.empty() && out.back() == ' ')) out += ch;
    return out;
}

std::string upper(std::string s) {
    for (char& ch : s)
        if (static_cast<unsigned char>(ch) < 128) ch = static_cast<char>(std::toupper(ch));
    return s;
}

double textWidth(const std::string& s, double size, double letterSpacing = 0) {
    return codepoints(s) * (0.6 * size + letterSpacing);
}

// draws text with its top at `top`, returns width
double htmlText(Svg& svg, const std::string& raw, double x, double top, double size, int weight,
                const std::string& color, double letterSpacing = 0) {
    const std::string s = collapseSpaces(raw);
    svg.text(s, x, top + size, size, weight, color, letterSpacing);
    return textWidth(s, size, letterSpacing);
}

// left sidebar
class Sidebar {
    static constexpr double kRight = 220 - 1 - 14, kTrack = kRight - 14;

public:
    explicit Sidebar(Svg& svg) : svg_(svg) {}

    void section(const std::string& title) {
        const double top = place(4, 10, 10);
        htmlText(svg_, upper(title), 14, top, 8, 400, theme::muted, 1.6);
    }

    void slider(const std::string& label, double value, double min, double max,
                const std::string& display, const char* color) {
        const double top = place(0, 21, 14);
        htmlText(svg_, label, 14, top, 9, 400, theme::mutedText);
        const std::string d = collapseSpaces(display);
        htmlText(svg_, d, kRight - textWidth(d, 11), top, 11, 600, color);
        const double pct = (value - min) / (max - min);
        svg_.rect(14, top + 19, kTrack, 2, theme::border, 1);
        svg_.rect(14, top + 19, kTrack * pct, 2, color, 1);
        svg_.circleGlow(14 + kTrack * pct, top + 20, 5, color, 6);
    }

    void toggle(const std::string& label, bool on, double marginBottom) {
        const double top = place(0, 14, marginBottom);
        htmlText(svg_, label, 14, top + 1.5, 9, 400, theme::mutedText);
        svg_.rect(kRight - 28, top, 28, 14, on ? theme::cyanDim : theme::border, 7);
        svg_.rect(kRight - 28 + (on ? 15 : 2), top + 2, 10, 10, "#ffffff", 5);
    }

    void derived(double rsKm, double iscoKmValue) {
        const double top = place(16, 0, 0);
        svg_.rect(14, top, kTrack, 1, theme::border);
        y_ = top + 1 + 12;
        prevMargin_ = 0;
        insideBlock_ = true;
        section("Schwarzschild");
        double line = place(0, 11, 4);
        double x = 14 + htmlText(svg_, "r_s = ", 14, line, 9, 400, theme::mutedText);
        htmlText(svg_, toFixed(rsKm, 1) + " km", x, line, 9, 400, theme::text);
        line = place(0, 11, 0);
        x = 14 + htmlText(svg_, "ISCO = ", 14, line, 9, 400, theme::mutedText);
        htmlText(svg_, toFixed(iscoKmValue, 1) + " km", x, line, 9, 400, theme::text);
    }

private:
    double place(double marginTop, double height, double marginBottom) {
        const double top = y_ + (insideBlock_ ? marginTop : std::max(prevMargin_, marginTop));
        insideBlock_ = false;
        y_ = top + height;
        prevMargin_ = marginBottom;
        return top;
    }

    Svg& svg_;
    double y_ = 44 + 14;
    double prevMargin_ = 0;
    bool insideBlock_ = true;
};

void drawHeader(Svg& svg, const AppState& s, const Ringdown& d) {
    svg.rect(0, 0, kW, 44, theme::surface);
    svg.rect(0, 43, kW, 1, theme::border);
    htmlText(svg, "KERR  QNM  RINGDOWN", 20, 9.5, 11, 600, theme::textBright, 1.65);
    htmlText(svg, "GRAVITATIONAL-WAVE  PERTURBATION  ANALYZER  ·  l=m=2  DOMINANT MODE", 20, 23.5, 8,
             400, theme::muted, 1.6);

    const std::string f = toFixed(d.qnm0.f_Hz, 1), tau = toFixed(d.qnm0.tau_s * 1000, 2);
    const std::vector<std::pair<std::string, const char*>> readout = {
        {f, theme::cyan}, {" Hz · ", theme::muted}, {tau, theme::amber2}, {" ms", theme::muted}};
    double readW = 0;
    for (const auto& part : readout) readW += textWidth(part.first, 8);
    double x = kW - 20 - readW;
    const double readoutX = x;
    for (const auto& part : readout) x += htmlText(svg, part.first, x, 16.5, 8, 400, part.second);

    const double dividerX = readoutX - 16 - 1;
    svg.rect(dividerX, 11.5, 1, 20, theme::border);

    double right = dividerX - 16;
    const auto& list = presets();
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        const std::string label = it->second.label;
        const double w = 20 + textWidth(label, 8, 0.8);
        const double bx = right - w;
        const bool active = it->first == s.preset;
        if (active) svg.rect(bx, 12.5, w, 18, std::string(theme::cyan) + "15", 2);
        svg.strokeRectInset(bx, 12.5, w, 18, active ? theme::cyanDim : theme::border, 2);
        htmlText(svg, label, bx + 10, 16.5, 8, 400, active ? theme::cyan : theme::mutedText, 0.8);
        right = bx - 6;
    }
}

void drawSidebar(Svg& svg, const AppState& s) {
    svg.rect(0, 44, 220, kH - 44, theme::panel);
    svg.rect(219, 44, 1, kH - 44, theme::border);
    Sidebar sb(svg);

    sb.section("Black Hole Parameters");
    sb.slider("Final Mass  M_f", s.massSolar, 5, 5000, jsNumber(s.massSolar) + " M☉", theme::cyan);
    sb.slider("Dimensionless Spin  χ", s.spin, 0, 0.98, toFixed(s.spin, 2), theme::amber2);

    sb.section("Observer / Source");
    sb.slider("Inclination  ι", s.inclination, 0, kPi / 2, toFixed(s.inclination * 180 / kPi, 0) + "°",
              theme::green);
    sb.slider("Initial Phase  φ₀", s.phi0, -kPi, kPi, toFixed(s.phi0, 2) + " rad", theme::muted);
    sb.slider("Strain Amplitude", s.strainLog10, -24, -17, "10^" + toFixed(s.strainLog10, 1), theme::cyan);

    sb.section("Mode Decomposition");
    sb.slider("Overtones  (n)", s.overtones, 0, 2, "n=0…" + std::to_string(s.overtones), theme::amber2);
    sb.toggle("l=m=3 sub-mode", s.show33, 14);

    sb.section("Detector Noise");
    sb.toggle("Inject noise", s.noiseEnabled, 10);
    if (s.noiseEnabled)
        sb.slider("Noise level", s.noiseScale, 0.01, 0.5, toFixed(s.noiseScale * 100, 0) + "%", theme::red);
    sb.slider("Detector SNR", s.detectorSensitivity, 1, 50, jsNumber(s.detectorSensitivity) + "×",
              theme::amber);

    sb.section("View");
    sb.slider("Time Zoom", s.zoom, 0.5, 8, jsNumber(s.zoom) + "×", theme::muted);

    sb.derived(schwarzschildRadiusKm(s.massSolar), iscoKm(s.massSolar, s.spin));
}

void drawTabs(Svg& svg, const AppState& s) {
    svg.rect(220, 44, 1220, 29, theme::surface);
    svg.rect(220, 72, 1220, 1, theme::border);
    double x = 224;
    for (const char* tab : {"waveform", "spectrogram", "power spectrum", "kerr parameters", "detector view"}) {
        const std::string label = upper(tab);
        const double w = 28 + textWidth(label, 9, 1.08);
        const bool active = s.tab == tab;
        htmlText(svg, label, x + 14, 52, 9, 400, active ? theme::textBright : theme::mutedText, 1.08);
        if (active) svg.rect(x, 71, w, 1, theme::cyan);
        x += w;
    }
}

void drawMetrics(Svg& svg, const AppState& s, const Ringdown& d) {
    svg.rect(220, 73, 1220, 41, theme::surface);
    svg.rect(220, 113, 1220, 1, theme::border);
    const bool hz = d.qnm0.f_Hz >= 1;
    const std::vector<std::array<std::string, 3>> items = {
        {"FINAL MASS",
         s.massSolar >= 1000 ? toFixed(s.massSolar / 1000, 2) + "k" : toFixed(s.massSolar, 1), "M☉"},
        {"SPIN  χ", toFixed(s.spin, 3), ""},
        {"f_QNM (2,2,0)", hz ? toFixed(d.qnm0.f_Hz, 1) : toFixed(d.qnm0.f_Hz * 1000, 3), hz ? "Hz" : "mHz"},
        {"τ_DAMP", toFixed(d.qnm0.tau_s * 1000, 2), "ms"},
        {"Q = πf·τ", toFixed(d.qnm0.Q, 3), ""},
        {"f_OT (n=1)", toFixed(d.qnm1.f_Hz, 1), "Hz"},
        {"f₃₃", toFixed(d.qnm33.f_Hz, 1), "Hz"},
        {"STRAIN  h₀", "10^" + toFixed(s.strainLog10, 1), ""},
    };
    double x = 220;
    for (const auto& item : items) {
        const std::string label = collapseSpaces(item[0]);
        const double labelW = textWidth(label, 7, 1.05);
        const double valueW = textWidth(item[1], 13), unitW = textWidth(item[2], 9);
        const double w = std::max(110.0, 32 + 1 + std::max(labelW, valueW + 2 + unitW));
        htmlText(svg, label, x + 16, 79, 7, 400, theme::muted, 1.05);
        htmlText(svg, item[1], x + 16, 90, 13, 600, theme::cyan);
        if (!item[2].empty()) htmlText(svg, item[2], x + 16 + valueW + 2, 94, 9, 600, theme::mutedText);
        svg.rect(x + w - 1, 73, 1, 40, theme::border);
        x += w;
    }
}

void drawFooter(Svg& svg, const AppState& s, const Ringdown& d) {
    svg.rect(220, kFooterTop, 1220, kH - kFooterTop, theme::surface);
    svg.rect(220, kFooterTop, 1220, 1, theme::border);
    const double f0 = d.qnm0.f_Hz;
    const std::string band = f0 < 50 ? "LISA band" : f0 < 500 ? "LIGO low-freq" : "LIGO band";
    const std::vector<std::pair<std::string, const char*>> parts = {
        {"Berti-Cardoso-Will (2006) QNM fitting formulae", theme::muted},
        {"·", theme::muted},
        {"Kerr perturbation theory · l=m=2, n=0.." + std::to_string(s.overtones), theme::muted},
        {"·", theme::muted},
        {"SR " + jsNumber(d.sampleRate) + " Hz · " + toFixed(d.duration * 1000, 1) + " ms window", theme::muted},
        {"·", theme::muted},
        {band, f0 > 500 ? theme::amber : theme::green},
    };
    double x = 234;
    for (const auto& p : parts) x += htmlText(svg, p.first, x, kFooterTop + 5, 8, 400, p.second) + 16;
}

// mode table next to the kerr plot
void drawModePanel(Svg& svg, const Ringdown& d) {
    const double px = 1180, py = kPlotTop, pw = 260, ph = kPlotH;
    svg.rect(px, py, pw, ph, theme::panel);
    svg.rect(px, py, 1, ph, theme::border);
    const double x0 = px + 1 + 16, cardW = pw - 1 - 32;
    double y = py + 16;
    double prevMargin = 0;
    bool first = true;
    auto place = [&](double mt, double h, double mb) {
        const double top = y + (first ? mt : std::max(prevMargin, mt));
        first = false;
        y = top + h;
        prevMargin = mb;
        return top;
    };
    auto section = [&](const std::string& t) {
        htmlText(svg, upper(t), x0, place(4, 10, 10), 8, 400, theme::muted, 1.6);
    };
    struct Row { std::string label, value, unit; };
    auto card = [&](const std::string& title, const char* color, const std::vector<Row>& rows) {
        const double h = 1 + 10 + 10 + 6 + 30.0 * rows.size() + 10 + 1;
        const double top = place(0, h, 12);
        svg.rect(x0, top, cardW, h, theme::bg, 2);
        svg.strokeRectInset(x0, top, cardW, h, theme::border, 2);
        const double ix = x0 + 11, right = x0 + cardW - 11;
        htmlText(svg, title, ix, top + 11, 8, 400, color, 0.8);
        double ry = top + 11 + 10 + 6;
        for (const Row& r : rows) {
            const double baseline = ry + 7 + 12;
            htmlText(svg, r.label, ix, baseline - 9, 9, 400, theme::mutedText, 0.72);
            const double unitW = textWidth(r.unit, 9, 0.48);
            const double valueW = textWidth(r.value, 12, 0.48);
            const double ux = right - unitW;
            htmlText(svg, r.value, ux - 3 - valueW, baseline - 12, 12, 600, color, 0.48);
            if (!r.unit.empty()) htmlText(svg, r.unit, ux, baseline - 9, 9, 600, theme::mutedText, 0.48);
            svg.rect(ix, ry + 29, right - ix, 1, theme::border);
            ry += 30;
        }
    };
    auto rowsFor = [](const QNM& q, const char* fl, const char* tl, const char* ql, int fDigits) {
        return std::vector<Row>{{fl, toFixed(q.f_Hz, fDigits), "Hz"},
                                {tl, toFixed(q.tau_s * 1000, 3), "ms"},
                                {ql, toFixed(q.Q, 4), ""}};
    };

    section("Mode Table  l=m=2");
    card("n=0 · dominant", theme::cyan, rowsFor(d.qnm0, "f_QNM", "τ_damp", "Q-factor", 2));
    card("n=1 · 1st overtone", theme::amber2, rowsFor(d.qnm1, "f_QNM", "τ_damp", "Q-factor", 2));
    section("Sub-dominant mode");
    card("l=m=3, n=0", theme::green, rowsFor(d.qnm33, "f₃₃", "τ₃₃", "Q₃₃", 2));
    section("References");
    const char* refs[] = {"Berti, Cardoso & Will (2006)", "PRD 73, 064030", "Fitting coefficients Table VIII", "",
                          "Leaver (1985) — exact QNM", "Echeverria (1989) — mass/spin",
                          "LIGO O1 detection: Abbott et al."};
    const double refTop = y + std::max(prevMargin, 0.0);
    for (int i = 0; i < 7; ++i)
        if (*refs[i]) htmlText(svg, refs[i], x0, refTop + 2 + i * 14.39, 8, 400, theme::muted);
}

}  // namespace

bool applyPreset(AppState& s, const std::string& key) {
    for (const auto& [k, p] : presets()) {
        if (k != key) continue;
        s.preset = k;
        s.massSolar = p.mass;
        s.spin = p.spin;
        s.strainLog10 = std::log10(p.A0);
        s.inclination = p.inclination;
        s.phi0 = p.phi0;
        return true;
    }
    return false;
}

Ringdown simulate(const AppState& s) {
    const Timing t = chooseTiming(s.massSolar, s.spin);
    RingdownParams p;
    p.massSolar = s.massSolar;
    p.spin = s.spin;
    p.A0 = std::pow(10.0, s.strainLog10);
    p.inclination = s.inclination;
    p.phi0 = s.phi0;
    p.noiseEnabled = s.noiseEnabled;
    p.noiseScale = s.noiseScale;
    p.overtones = s.overtones;
    p.show33 = s.show33;
    p.sampleRate = t.sampleRate;
    p.duration = t.duration;
    p.mergerFrac = t.mergerFrac;
    p.seed = s.seed;
    return generateRingdown(p);
}

std::string renderApp(const AppState& s, View view, const std::string& fontDir) {
    const Ringdown d = simulate(s);
    Svg svg(kW, kH);
    svg.embedFonts(fontDir);
    svg.rect(0, 0, kW, kH, theme::bg);

    const double ax = 220, ay = kPlotTop, aw = 1220, ah = kPlotH;
    if (s.tab == "kerr parameters") {
        svg.beginCanvas(ax, ay, aw - 260, ah);
        drawKerrDiagram(svg, aw - 260, ah, std::min(s.massSolar, 200.0), s.spin);
        svg.endCanvas();
        drawModePanel(svg, d);
    } else {
        svg.beginCanvas(ax, ay, aw, ah);
        if (s.tab == "spectrogram") drawSpectrogram(svg, aw, ah, d);
        else if (s.tab == "power spectrum") drawPowerSpectrum(svg, aw, ah, d);
        else if (s.tab == "detector view") drawDetectorView(svg, aw, ah, d, s.detectorSensitivity);
        else drawWaveform(svg, aw, ah, d, s.zoom);
        svg.endCanvas();
    }

    if (view == View::Plot)
        return svg.str(fmtNum(ax) + " " + fmtNum(ay) + " " + fmtNum(aw) + " " + fmtNum(ah));

    drawHeader(svg, s, d);
    drawSidebar(svg, s);
    drawTabs(svg, s);
    drawMetrics(svg, s, d);
    drawFooter(svg, s, d);
    return svg.str();
}

}  // namespace kerr
