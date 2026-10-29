// QNM fits from Berti, Cardoso & Will (2006), PRD 73, 064030:
//   M*omega = f1 + f2*(1-a)^f3,  Q = q1 + q2*(1-a)^q3,  tau = Q/(pi*f)
#pragma once

namespace kerr {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSolarMassSeconds = 4.9255e-6;  // G M_sun / c^3 in seconds
constexpr double kSolarMassKg = 1.989e30;
constexpr double kG = 6.674e-11;
constexpr double kSpeedOfLight = 3e8;

struct QNM {
    double f_Hz;
    double tau_s;
    double Q;
};

// l=m=2 mode, overtone 0 or 1
QNM kerrQNM(double massSolar, double spin, int overtone = 0);

// l=m=3, n=0 mode
QNM kerrQNM33(double massSolar, double spin);

// Schwarzschild radius and ISCO in km
double schwarzschildRadiusKm(double massSolar);
double iscoKm(double massSolar, double spin);

}  // namespace kerr
