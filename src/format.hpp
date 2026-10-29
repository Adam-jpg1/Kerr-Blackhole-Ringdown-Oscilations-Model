// number formatting helpers
#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <string>

namespace kerr {

// same rounding as JS toFixed
inline std::string toFixed(double x, int digits) {
    if (std::isnan(x)) return "NaN";
    char buf[512];
    std::snprintf(buf, sizeof buf, "%.*f", digits + 40, std::fabs(x));
    std::string s(buf);
    const std::size_t dot = s.find('.');
    const std::string tail = s.substr(dot + 1 + digits);
    bool tie = !tail.empty() && tail[0] == '5' &&
               tail.find_first_not_of('0', 1) == std::string::npos;
    double v = std::fabs(x);
    if (tie) v += 0.5 * std::pow(10.0, -digits);
    std::snprintf(buf, sizeof buf, "%.*f", digits, v);
    std::string out(buf);
    if (x < 0) out = "-" + out;
    return out;
}

// shortest form, e.g. 68.5 or 10
inline std::string jsNumber(double x) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof buf, x);
    return std::string(buf, res.ptr);
}

// utf-8 character count
inline int codepoints(const std::string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

}  // namespace kerr
