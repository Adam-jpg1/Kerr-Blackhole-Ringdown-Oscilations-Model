#include "app.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
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

constexpr double kW = 1440, kH = 900;
constexpr double kPlotTop = 114, kPlotH = kH - kPlotTop;
const std::string kWhite = theme::white;
const std::string kDimWhite = std::string(theme::white) + "8c";

std::string upper(std::string s) {
    for (char& ch : s)
        if (static_cast<unsigned char>(ch) < 128) ch = static_cast<char>(std::toupper(ch));
    return s;
}

// rough width of Times New Roman text, used to size buttons and tabs
double estWidth(const std::string& s, double size) {
    double w = 0;
    for (unsigned char ch : s) {
        if ((ch & 0xC0) == 0x80) continue;
        if (ch == ' ') w += 0.25;
        else if (std::isupper(ch)) w += 0.68;
        else if (std::isdigit(ch)) w += 0.5;
        else if (std::islower(ch)) w += 0.45;
        else w += 0.35;
    }
    return w * size;
}

// left sidebar
class Sidebar {
    static constexpr double kLeft = 14, kRight = 220 - 1 - 14, kTrack = kRight - kLeft;

public:
    explicit Sidebar(Svg& svg) : svg_(svg) {}

    void section(const std::string& title) {
        const double top = place(4, 12, 10);
        svg_.text(upper(title), kLeft, top + 10, 10, 600, kWhite, "start", 1);
    }

    void slider(const std::string& label, double value, double min, double max, const std::string& display) {
        const double top = place(0, 24, 14);
        svg_.text(label, kLeft, top + 11, 12, 400, kWhite);
        svg_.text(display, kRight, top + 11, 12, 600, kWhite, "end");
        const double pct = std::clamp((value - min) / (max - min), 0.0, 1.0);
        svg_.rect(kLeft, top + 20, kTrack, 2, theme::border, 1);
        svg_.rect(kLeft, top + 20, kTrack * pct, 2, kWhite, 1);
        svg_.rect(kLeft + kTrack * pct - 5, top + 16, 10, 10, kWhite, 5);
    }

    void toggle(const std::string& label, bool on, double marginBottom) {
        const double top = place(0, 14, marginBottom);
        svg_.text(label, kLeft, top + 11, 12, 400, kWhite);
        svg_.rect(kRight - 28, top, 28, 14, on ? kWhite : theme::border, 7);
        svg_.rect(kRight - 28 + (on ? 15 : 2), top + 2, 10, 10, on ? theme::bg : kWhite, 5);
    }

private:
    double place(double marginTop, double height, double marginBottom) {
        const double top = y_ + (first_ ? marginTop : std::max(prevMargin_, marginTop));
        first_ = false;
        y_ = top + height;
        prevMargin_ = marginBottom;
        return top;
    }

    Svg& svg_;
    double y_ = 44 + 14;
    double prevMargin_ = 0;
    bool first_ = true;
};

void drawHeader(Svg& svg, const AppState& s) {
    svg.rect(0, 0, kW, 44, theme::surface);
    svg.rect(0, 43, kW, 1, theme::border);
    svg.text("KERR QNM RINGDOWN", 20, 27, 15, 600, kWhite, "start", 1);

    double right = kW - 20;
    const auto& list = presets();
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        const std::string label = it->second.label;
        const double w = 20 + estWidth(label, 11);
        const double bx = right - w;
        const bool active = it->first == s.preset;
        if (active) svg.rect(bx, 11, w, 22, kWhite + "1f", 2);
        svg.strokeRectInset(bx, 11, w, 22, active ? kWhite : theme::border, 2);
        svg.text(label, bx + w / 2, 26, 11, 400, kWhite, "middle");
        right = bx - 6;
    }
}

void drawSidebar(Svg& svg, const AppState& s) {
    svg.rect(0, 44, 220, kH - 44, theme::panel);
    svg.rect(219, 44, 1, kH - 44, theme::border);
    Sidebar sb(svg);

    sb.section("Black Hole");
    sb.slider("Final mass", s.massSolar, 5, 5000, jsNumber(s.massSolar) + " M☉");
    sb.slider("Spin", s.spin, 0, 0.98, toFixed(s.spin, 2));

    sb.section("Source");
    sb.slider("Inclination", s.inclination, 0, kPi / 2, toFixed(s.inclination * 180 / kPi, 0) + "°");
    sb.slider("Initial phase", s.phi0, -kPi, kPi, toFixed(s.phi0, 2) + " rad");
    sb.slider("Strain amplitude", s.strainLog10, -24, -17, "10^" + toFixed(s.strainLog10, 1));

    sb.section("Modes");
    sb.slider("Overtones", s.overtones, 0, 2, s.overtones == 0 ? "n = 0" : "n = 0-" + std::to_string(s.overtones));
    sb.toggle("l=m=3 mode", s.show33, 14);

    sb.section("Noise");
    sb.toggle("Detector noise", s.noiseEnabled, 10);
    if (s.noiseEnabled) sb.slider("Noise level", s.noiseScale, 0.01, 0.5, toFixed(s.noiseScale * 100, 0) + "%");
    sb.slider("Detector SNR", s.detectorSensitivity, 1, 50, jsNumber(s.detectorSensitivity) + "x");

    sb.section("View");
    sb.slider("Time zoom", s.zoom, 0.5, 8, jsNumber(s.zoom) + "x");
}

void drawTabs(Svg& svg, const AppState& s) {
    svg.rect(220, 44, 1220, 29, theme::surface);
    svg.rect(220, 72, 1220, 1, theme::border);
    double x = 224;
    for (const char* tab : {"waveform", "spectrogram", "power spectrum", "kerr parameters", "detector view"}) {
        const std::string label = upper(tab);
        const double w = 28 + estWidth(label, 11);
        const bool active = s.tab == tab;
        svg.text(label, x + w / 2, 62, 11, 400, active ? kWhite : kDimWhite, "middle");
        if (active) svg.rect(x, 71, w, 1, kWhite);
        x += w;
    }
}

void drawMetrics(Svg& svg, const AppState& s, const Ringdown& d) {
    svg.rect(220, 73, 1220, 41, theme::surface);
    svg.rect(220, 113, 1220, 1, theme::border);
    const bool hz = d.qnm0.f_Hz >= 1;
    const std::vector<std::array<std::string, 3>> items = {
        {"Final mass", s.massSolar >= 1000 ? toFixed(s.massSolar / 1000, 2) + "k" : toFixed(s.massSolar, 1), "M☉"},
        {"Spin", toFixed(s.spin, 3), ""},
        {"f (2,2,0)", hz ? toFixed(d.qnm0.f_Hz, 1) : toFixed(d.qnm0.f_Hz * 1000, 3), hz ? "Hz" : "mHz"},
        {"Damping time", toFixed(d.qnm0.tau_s * 1000, 2), "ms"},
        {"Q", toFixed(d.qnm0.Q, 3), ""},
        {"f (2,2,1)", toFixed(d.qnm1.f_Hz, 1), "Hz"},
        {"f (3,3,0)", toFixed(d.qnm33.f_Hz, 1), "Hz"},
    };
    const double w = 125;
    double x = 220;
    for (const auto& item : items) {
        svg.text(item[0], x + 16, 88, 10, 400, kWhite);
        svg.textWithUnit(item[1], item[2], x + 16, 106, 15, 11, kWhite);
        svg.rect(x + w - 1, 73, 1, 40, theme::border);
        x += w;
    }
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
        svg.text(upper(t), x0, place(4, 12, 10) + 10, 10, 600, kWhite, "start", 1);
    };
    struct Row { std::string label, value, unit; };
    auto card = [&](const std::string& title, const std::vector<Row>& rows) {
        const double h = 1 + 10 + 12 + 6 + 30.0 * rows.size() + 10 + 1;
        const double top = place(0, h, 12);
        svg.rect(x0, top, cardW, h, theme::bg, 2);
        svg.strokeRectInset(x0, top, cardW, h, theme::border, 2);
        const double ix = x0 + 11, right = x0 + cardW - 11;
        svg.text(title, ix, top + 21, 12, 600, kWhite);
        double ry = top + 11 + 12 + 6;
        for (const Row& r : rows) {
            svg.text(r.label, ix, ry + 20, 12, 400, kWhite);
            svg.textWithUnit(r.value, r.unit, right, ry + 20, 14, 10, kWhite, "end");
            svg.rect(ix, ry + 29, right - ix, 1, theme::border);
            ry += 30;
        }
    };
    auto rowsFor = [](const QNM& q) {
        return std::vector<Row>{{"Frequency", toFixed(q.f_Hz, 2), "Hz"},
                                {"Damping time", toFixed(q.tau_s * 1000, 3), "ms"},
                                {"Q", toFixed(q.Q, 4), ""}};
    };

    section("Modes");
    card("l=m=2, n=0", rowsFor(d.qnm0));
    card("l=m=2, n=1", rowsFor(d.qnm1));
    card("l=m=3, n=0", rowsFor(d.qnm33));
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

std::string renderApp(const AppState& s, View view) {
    const Ringdown d = simulate(s);
    Svg svg(kW, kH);
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

    drawHeader(svg, s);
    drawSidebar(svg, s);
    drawTabs(svg, s);
    drawMetrics(svg, s, d);
    return svg.str();
}

}  // namespace kerr
