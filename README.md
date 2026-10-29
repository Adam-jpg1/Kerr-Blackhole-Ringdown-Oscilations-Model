# Kerr Ringdown Simulator

C++ version of the Kerr black hole ringdown simulator from our NSRI paper
"Computational Modeling of Kerr Black Hole Ringdown Oscillations".

It generates the inspiral/merger/ringdown waveform for a given final mass and spin
using QNM fits from Berti, Cardoso & Will (2006), then makes the waveform, power
spectrum, spectrogram, Kerr parameter and detector plots as SVG files.

## Building

```
cmake -S . -B build
cmake --build build --config Release
```

or

```
g++ -std=c++17 -O2 -o kerr_ringdown src/*.cpp
```

## Usage

```
kerr_ringdown --paper figures
kerr_ringdown --report
kerr_ringdown --mass 30 --spin 0.9 --tab "power spectrum" -o spectrum.svg
kerr_ringdown --csv waveform.csv
kerr_ringdown --help
```

`--paper` writes the figures from the results section. The `fonts` folder needs to
be next to the executable (cmake copies it).

Baseline (68.5 solar masses, spin 0.69): f = 250.36 Hz, tau = 4.126 ms, Q = 3.2451.

Font: IBM Plex Mono, SIL Open Font License (see fonts/).
