// Kerr ringdown simulator
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "app.hpp"
#include "format.hpp"

namespace fs = std::filesystem;
using namespace kerr;

namespace {

// spins used for figure 3
constexpr double kFig3HighSpin = 0.95;
constexpr double kFig3LowSpin = 0.10;

void usage() {
    std::puts(
        "Kerr QNM Ringdown Simulator\n"
        "\n"
        "Usage: kerr_ringdown [options]\n"
        "  --paper DIR           write every Results figure (full screen + plot only) to DIR\n"
        "  -o, --out FILE.svg    write one view (default: kerr_ringdown.svg)\n"
        "  --view full|plot      whole simulator screen, or just the plot (default: full)\n"
        "  --tab NAME            waveform | spectrogram | \"power spectrum\" | \"kerr parameters\"\n"
        "                        | \"detector view\"\n"
        "  --preset NAME         GW150914 | HighSpin | IMBH | SMBH\n"
        "  --mass M              final mass in solar masses\n"
        "  --spin CHI            dimensionless spin, 0 to 0.98\n"
        "  --strain LOG10        peak strain as a power of ten (e.g. -21)\n"
        "  --inclination DEG     observer inclination in degrees\n"
        "  --phase RAD           initial phase in radians\n"
        "  --overtones N         0 = fundamental only, 1 = add the first overtone\n"
        "  --mode33              add the l=m=3 sub-dominant mode\n"
        "  --noise LEVEL         inject detector noise (e.g. 0.05 = 5%)\n"
        "  --snr X               detector SNR for the detector view\n"
        "  --zoom X              time zoom for the waveform view\n"
        "  --seed N              noise seed (default 1)\n"
        "  --report              print the QNM mode table\n"
        "  --csv FILE            export t (s) and h(t) as CSV");
}

bool writeFile(const fs::path& path, const std::string& text) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
    return static_cast<bool>(out);
}

void printReport(const AppState& s) {
    const Ringdown d = simulate(s);
    std::printf("Final mass        %s M_sun\n", jsNumber(s.massSolar).c_str());
    std::printf("Spin chi          %s\n", toFixed(s.spin, 3).c_str());
    std::printf("Sample rate       %s Hz, window %s ms, %zu samples\n", jsNumber(d.sampleRate).c_str(),
                toFixed(d.duration * 1000, 2).c_str(), d.strain.size());
    std::printf("\n%-22s %12s %12s %10s\n", "mode", "f (Hz)", "tau (ms)", "Q");
    auto row = [](const char* name, const QNM& q) {
        std::printf("%-22s %12s %12s %10s\n", name, toFixed(q.f_Hz, 2).c_str(),
                    toFixed(q.tau_s * 1000, 3).c_str(), toFixed(q.Q, 4).c_str());
    };
    row("l=m=2, n=0 (dominant)", d.qnm0);
    row("l=m=2, n=1 (overtone)", d.qnm1);
    row("l=m=3, n=0", d.qnm33);
}

void writePaperFigures(const fs::path& dir) {
    auto baseline = [] {
        AppState s;
        applyPreset(s, "GW150914");
        s.overtones = 0;
        return s;
    };
    struct Fig {
        const char* name;
        AppState state;
    };
    std::vector<Fig> figs;
    { AppState s = baseline(); figs.push_back({"fig1_waveform", s}); }
    { AppState s = baseline(); s.tab = "power spectrum"; figs.push_back({"fig2_power_spectrum", s}); }
    { AppState s = baseline(); s.spin = kFig3HighSpin; figs.push_back({"fig3a_high_spin", s}); }
    { AppState s = baseline(); s.spin = kFig3LowSpin; figs.push_back({"fig3b_low_spin", s}); }
    { AppState s = baseline(); s.noiseEnabled = true; s.noiseScale = 0.05; figs.push_back({"fig4_noise", s}); }
    { AppState s = baseline(); s.tab = "kerr parameters"; figs.push_back({"fig5_kerr_parameters", s}); }
    { AppState s = baseline(); s.massSolar = 30; figs.push_back({"fig6a_mass_30", s}); }
    { AppState s = baseline(); figs.push_back({"fig6b_mass_68_5", s}); }
    { AppState s = baseline(); s.massSolar = 150; figs.push_back({"fig6c_mass_150", s}); }

    for (const Fig& f : figs) {
        writeFile(dir / (std::string(f.name) + "_full.svg"), renderApp(f.state, View::Full));
        writeFile(dir / (std::string(f.name) + "_plot.svg"), renderApp(f.state, View::Plot));
        std::printf("wrote %s\n", (dir / f.name).string().c_str());
    }
    std::printf("\nFigure 5 mode table (Mf = 68.5, chi = 0.69):\n");
    printReport(baseline());
}

}  // namespace

int main(int argc, char** argv) {
    AppState s;
    std::string out = "kerr_ringdown.svg", paperDir, csv;
    View view = View::Full;
    bool report = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "--paper") paperDir = next();
        else if (a == "-o" || a == "--out") out = next();
        else if (a == "--view") view = next() == "plot" ? View::Plot : View::Full;
        else if (a == "--tab") s.tab = next();
        else if (a == "--preset") {
            const std::string p = next();
            if (!applyPreset(s, p)) { std::fprintf(stderr, "unknown preset %s\n", p.c_str()); return 2; }
        }
        else if (a == "--mass") s.massSolar = std::stod(next());
        else if (a == "--spin") s.spin = std::stod(next());
        else if (a == "--strain") s.strainLog10 = std::stod(next());
        else if (a == "--inclination") s.inclination = std::stod(next()) * kPi / 180;
        else if (a == "--phase") s.phi0 = std::stod(next());
        else if (a == "--overtones") s.overtones = std::stoi(next());
        else if (a == "--mode33") s.show33 = true;
        else if (a == "--noise") { s.noiseEnabled = true; s.noiseScale = std::stod(next()); }
        else if (a == "--snr") s.detectorSensitivity = std::stod(next());
        else if (a == "--zoom") s.zoom = std::stod(next());
        else if (a == "--seed") s.seed = static_cast<std::uint32_t>(std::stoul(next()));
        else if (a == "--report") report = true;
        else if (a == "--csv") csv = next();
        else { std::fprintf(stderr, "unknown option %s (try --help)\n", a.c_str()); return 2; }
    }

    if (!paperDir.empty()) {
        writePaperFigures(paperDir);
        return 0;
    }
    if (report) {
        printReport(s);
        return 0;
    }
    if (!csv.empty()) {
        const Ringdown d = simulate(s);
        std::ofstream f(csv);
        f << "t_s,strain\n";
        char line[96];
        for (std::size_t i = 0; i < d.strain.size(); ++i) {
            std::snprintf(line, sizeof line, "%.9e,%.9e\n", d.times[i], d.strain[i]);
            f << line;
        }
        std::printf("wrote %s (%zu samples)\n", csv.c_str(), d.strain.size());
        return 0;
    }
    if (!writeFile(out, renderApp(s, view))) {
        std::fprintf(stderr, "could not write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s\n", out.c_str());
    return 0;
}
