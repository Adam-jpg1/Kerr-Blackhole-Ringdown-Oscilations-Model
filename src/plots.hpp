// plot views
#pragma once

#include "signal.hpp"
#include "svg.hpp"

namespace kerr {

void drawWaveform(Svg& c, double W, double H, const Ringdown& d, double zoom);
void drawSpectrogram(Svg& c, double W, double H, const Ringdown& d);
void drawPowerSpectrum(Svg& c, double W, double H, const Ringdown& d);
void drawKerrDiagram(Svg& c, double W, double H, double massSolar, double spin);
void drawDetectorView(Svg& c, double W, double H, const Ringdown& d, double detectorSensitivity);

}  // namespace kerr
