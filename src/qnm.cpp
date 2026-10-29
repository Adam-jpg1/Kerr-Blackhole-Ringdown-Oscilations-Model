#include "qnm.hpp"

#include <algorithm>
#include <cmath>

namespace kerr {
namespace {

// Berti et al. 2006 coefficients
constexpr double F1 = 1.5251, F2 = -1.1568, F3 = 0.1292;  // l=m=2, n=0
constexpr double Q1 = 0.7000, Q2 = 1.4187, Q3 = -0.4990;
constexpr double F1_N1 = 1.3556, F2_N1 = -1.0157, F3_N1 = 0.0765;  // l=m=2, n=1
constexpr double Q1_N1 = 0.2900, Q2_N1 = 0.4991, Q3_N1 = -0.1980;
constexpr double F1_33 = 1.8956, F2_33 = -1.3043, F3_33 = 0.1818;  // l=m=3, n=0
constexpr double Q1_33 = 0.9000, Q2_33 = 2.3430, Q3_33 = -0.4810;

double clampSpin(double a) { return std::min(std::max(a, 0.0), 0.98); }

QNM fromFit(double massSolar, double fM, double q) {
    const double massSeconds = massSolar * kSolarMassSeconds;
    const double f = fM / (2.0 * kPi * massSeconds);
    return {f, q / (kPi * f), q};
}

}  // namespace

QNM kerrQNM(double massSolar, double spin, int overtone) {
    const double a = clampSpin(spin);
    if (overtone == 0) {
        return fromFit(massSolar, F1 + F2 * std::pow(1 - a, F3), Q1 + Q2 * std::pow(1 - a, Q3));
    }
    return fromFit(massSolar, F1_N1 + F2_N1 * std::pow(1 - a, F3_N1),
                   Q1_N1 + Q2_N1 * std::pow(1 - a, Q3_N1));
}

QNM kerrQNM33(double massSolar, double spin) {
    const double a = clampSpin(spin);
    return fromFit(massSolar, F1_33 + F2_33 * std::pow(1 - a, F3_33),
                   Q1_33 + Q2_33 * std::pow(1 - a, Q3_33));
}

double schwarzschildRadiusKm(double massSolar) {
    return (massSolar * kSolarMassKg * 2 * kG) / (kSpeedOfLight * kSpeedOfLight) / 1000.0;
}

double iscoKm(double massSolar, double spin) {
    return 3 * schwarzschildRadiusKm(massSolar) * (1 + std::sqrt(1 - spin * spin));
}

}  // namespace kerr
