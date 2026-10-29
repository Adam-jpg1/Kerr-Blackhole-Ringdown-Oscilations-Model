// full simulator screen (1440x900)
#pragma once

#include <cstdint>
#include <string>

#include "signal.hpp"

namespace kerr {

struct AppState {
    std::string preset = "GW150914";
    double massSolar = 68.5;
    double spin = 0.69;
    double strainLog10 = -21;
    double inclination = 0.4;  // rad
    double phi0 = 0;           // rad
    bool noiseEnabled = false;
    double noiseScale = 0.05;
    int overtones = 1;
    bool show33 = false;
    double detectorSensitivity = 10;
    double zoom = 1;
    std::string tab = "waveform";
    std::uint32_t seed = 1;
};

// GW150914, HighSpin, IMBH, SMBH
bool applyPreset(AppState& s, const std::string& key);

Ringdown simulate(const AppState& s);

enum class View { Full, Plot };

std::string renderApp(const AppState& s, View view, const std::string& fontDir);

}  // namespace kerr
