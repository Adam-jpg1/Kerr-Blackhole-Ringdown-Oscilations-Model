#include "svg.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace kerr {

std::string fmtNum(double v) {
    if (std::fabs(v) < 5e-5) v = 0;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.4f", v);
    std::string s(buf);
    s.erase(s.find_last_not_of('0') + 1);
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

Rgba parseColor(const std::string& css) {
    Rgba c;
    if (!css.empty() && css[0] == '#') {
        auto hex = [&](std::size_t i) { return std::stoi(css.substr(i, 2), nullptr, 16); };
        c.r = hex(1);
        c.g = hex(3);
        c.b = hex(5);
        if (css.size() >= 9) c.a = hex(7) / 255.0;
    } else if (css.rfind("rgb", 0) == 0) {
        std::sscanf(css.c_str(), "rgb(%d,%d,%d)", &c.r, &c.g, &c.b);
    }
    return c;
}

namespace {

std::string escapeXml(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch;
        }
    }
    return out;
}

std::string rgb(const Rgba& c) {
    return "rgb(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + ")";
}

std::string base64(const std::string& data) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        unsigned v = (unsigned char)data[i] << 16 | (unsigned char)data[i + 1] << 8 |
                     (unsigned char)data[i + 2];
        out += t[v >> 18 & 63];
        out += t[v >> 12 & 63];
        out += t[v >> 6 & 63];
        out += t[v & 63];
    }
    if (i + 1 == data.size()) {
        unsigned v = (unsigned char)data[i] << 16;
        out += t[v >> 18 & 63];
        out += t[v >> 12 & 63];
        out += "==";
    } else if (i + 2 == data.size()) {
        unsigned v = (unsigned char)data[i] << 16 | (unsigned char)data[i + 1] << 8;
        out += t[v >> 18 & 63];
        out += t[v >> 12 & 63];
        out += t[v >> 6 & 63];
        out += '=';
    }
    return out;
}

const char* kFontFamily = "'Times New Roman', Times, serif";

}  // namespace

Svg::Svg(double width, double height) : width_(width), height_(height) {}

void Svg::save() {
    stack_.push_back({fillStyle, strokeStyle, shadowColor, textAlign, lineWidth, shadowBlur,
                      fontSize, fontWeight, dash_, transform_});
}

void Svg::restore() {
    if (stack_.empty()) return;
    const State& s = stack_.back();
    fillStyle = s.fillStyle;
    strokeStyle = s.strokeStyle;
    shadowColor = s.shadowColor;
    textAlign = s.textAlign;
    lineWidth = s.lineWidth;
    shadowBlur = s.shadowBlur;
    fontSize = s.fontSize;
    fontWeight = s.fontWeight;
    dash_ = s.dash;
    transform_ = s.transform;
    stack_.pop_back();
}

void Svg::translate(double x, double y) {
    transform_ += " translate(" + fmtNum(x) + " " + fmtNum(y) + ")";
}

void Svg::rotate(double radians) {
    transform_ += " rotate(" + fmtNum(radians * 180.0 / 3.14159265358979323846) + ")";
}

std::string Svg::transformAttr() const {
    return transform_.empty() ? "" : " transform=\"" + transform_.substr(1) + "\"";
}

std::string Svg::paint(const std::string& attr, const std::string& css) const {
    const Rgba c = parseColor(css);
    std::string s = " " + attr + "=\"" + rgb(c) + "\"";
    if (c.a < 1.0) s += " " + attr + "-opacity=\"" + fmtNum(c.a) + "\"";
    return s;
}

// shadow/glow filter (blur sigma = shadowBlur / 2)
std::string Svg::shadowFilter() {
    if (shadowBlur <= 0) return "";
    const Rgba c = parseColor(shadowColor);
    if (c.a <= 0) return "";
    const bool inCanvas = canvasW_ > 0;
    const double w = inCanvas ? canvasW_ : width_, h = inCanvas ? canvasH_ : height_;
    const std::string key = shadowColor + "|" + fmtNum(shadowBlur) + "|" + fmtNum(w) + "|" + fmtNum(h);
    auto it = filters_.find(key);
    if (it == filters_.end()) {
        const std::string id = "glow" + std::to_string(nextId_++);
        defs_ << "<filter id=\"" << id << "\" filterUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\""
              << fmtNum(w) << "\" height=\"" << fmtNum(h)
              << "\" color-interpolation-filters=\"sRGB\">"
              << "<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"" << fmtNum(shadowBlur / 2)
              << "\" result=\"b\"/><feFlood flood-color=\"" << rgb(c) << "\" flood-opacity=\""
              << fmtNum(c.a) << "\"/><feComposite operator=\"in\" in2=\"b\"/>"
              << "<feMerge><feMergeNode/><feMergeNode in=\"SourceGraphic\"/></feMerge></filter>\n";
        it = filters_.emplace(key, id).first;
    }
    return " filter=\"url(#" + it->second + ")\"";
}

void Svg::beginPath() { path_.clear(); }
void Svg::moveTo(double x, double y) { path_ += "M" + fmtNum(x) + " " + fmtNum(y); }
void Svg::lineTo(double x, double y) { path_ += "L" + fmtNum(x) + " " + fmtNum(y); }

void Svg::arc(double cx, double cy, double r) {
    path_ += "M" + fmtNum(cx + r) + " " + fmtNum(cy) + "A" + fmtNum(r) + " " + fmtNum(r) +
             " 0 1 1 " + fmtNum(cx - r) + " " + fmtNum(cy) + "A" + fmtNum(r) + " " + fmtNum(r) +
             " 0 1 1 " + fmtNum(cx + r) + " " + fmtNum(cy) + "Z";
}

void Svg::stroke() {
    if (path_.empty()) return;
    body_ << "<path d=\"" << path_ << "\" fill=\"none\"" << paint("stroke", strokeStyle)
          << " stroke-width=\"" << fmtNum(lineWidth) << "\" stroke-miterlimit=\"10\"";
    if (!dash_.empty()) {
        body_ << " stroke-dasharray=\"";
        for (std::size_t i = 0; i < dash_.size(); ++i) body_ << (i ? " " : "") << fmtNum(dash_[i]);
        body_ << "\"";
    }
    body_ << shadowFilter() << transformAttr() << "/>\n";
}

void Svg::fill() {
    if (path_.empty()) return;
    body_ << "<path d=\"" << path_ << "\"" << paint("fill", fillStyle) << shadowFilter()
          << transformAttr() << "/>\n";
}

void Svg::fillRect(double x, double y, double w, double h) {
    body_ << "<rect x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" width=\"" << fmtNum(w)
          << "\" height=\"" << fmtNum(h) << "\"" << paint("fill", fillStyle) << shadowFilter()
          << transformAttr() << "/>\n";
}

void Svg::fillText(const std::string& s, double x, double y) {
    std::string anchor = "start";
    if (textAlign == "center") anchor = "middle";
    else if (textAlign == "right" || textAlign == "end") anchor = "end";
    body_ << "<text x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" font-family=\"" << kFontFamily
          << "\" font-size=\"" << fmtNum(fontSize) << "\"";
    if (fontWeight != 400) body_ << " font-weight=\"" << fontWeight << "\"";
    if (anchor != "start") body_ << " text-anchor=\"" << anchor << "\"";
    body_ << paint("fill", fillStyle) << shadowFilter() << transformAttr() << ">" << escapeXml(s)
          << "</text>\n";
}

void Svg::beginCanvas(double x, double y, double w, double h) {
    const std::string id = "clip" + std::to_string(nextId_++);
    defs_ << "<clipPath id=\"" << id << "\"><rect x=\"0\" y=\"0\" width=\"" << fmtNum(w)
          << "\" height=\"" << fmtNum(h) << "\"/></clipPath>\n";
    body_ << "<g transform=\"translate(" << fmtNum(x) << " " << fmtNum(y) << ")\" clip-path=\"url(#"
          << id << ")\">\n";
    canvasW_ = w;
    canvasH_ = h;
    fillStyle = strokeStyle = "#000000";
    lineWidth = 1;
    shadowColor = "#00000000";
    shadowBlur = 0;
    fontSize = 10;
    fontWeight = 400;
    textAlign = "start";
    dash_.clear();
    transform_.clear();
}

void Svg::endCanvas() {
    body_ << "</g>\n";
    canvasW_ = canvasH_ = 0;
    shadowBlur = 0;
    transform_.clear();
}

void Svg::rect(double x, double y, double w, double h, const std::string& fill, double radius) {
    body_ << "<rect x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" width=\"" << fmtNum(w)
          << "\" height=\"" << fmtNum(h) << "\"";
    if (radius > 0) body_ << " rx=\"" << fmtNum(radius) << "\"";
    body_ << paint("fill", fill) << "/>\n";
}

void Svg::strokeRectInset(double x, double y, double w, double h, const std::string& color,
                          double radius) {
    body_ << "<rect x=\"" << fmtNum(x + 0.5) << "\" y=\"" << fmtNum(y + 0.5) << "\" width=\""
          << fmtNum(w - 1) << "\" height=\"" << fmtNum(h - 1) << "\"";
    if (radius > 0) body_ << " rx=\"" << fmtNum(radius - 0.5) << "\"";
    body_ << " fill=\"none\"" << paint("stroke", color) << " stroke-width=\"1\"/>\n";
}

void Svg::text(const std::string& s, double x, double baseline, double size, int weight,
               const std::string& color, const std::string& anchor, double letterSpacing) {
    body_ << "<text x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(baseline) << "\" font-family=\""
          << kFontFamily << "\" font-size=\"" << fmtNum(size) << "\"";
    if (weight != 400) body_ << " font-weight=\"" << weight << "\"";
    if (anchor != "start") body_ << " text-anchor=\"" << anchor << "\"";
    if (letterSpacing != 0) body_ << " letter-spacing=\"" << fmtNum(letterSpacing) << "\"";
    body_ << paint("fill", color) << " xml:space=\"preserve\">" << escapeXml(s) << "</text>\n";
}

void Svg::textWithUnit(const std::string& value, const std::string& unit, double x, double baseline,
                       double size, double unitSize, const std::string& color,
                       const std::string& anchor) {
    body_ << "<text x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(baseline) << "\" font-family=\""
          << kFontFamily << "\" font-size=\"" << fmtNum(size) << "\" font-weight=\"600\"";
    if (anchor != "start") body_ << " text-anchor=\"" << anchor << "\"";
    body_ << paint("fill", color) << ">" << escapeXml(value);
    if (!unit.empty())
        body_ << "<tspan dx=\"3\" font-size=\"" << fmtNum(unitSize) << "\" font-weight=\"400\">"
              << escapeXml(unit) << "</tspan>";
    body_ << "</text>\n";
}

void Svg::circleGlow(double cx, double cy, double r, const std::string& color, double blur) {
    const std::string savedColor = shadowColor;
    const double savedBlur = shadowBlur;
    shadowColor = color;
    shadowBlur = blur;
    body_ << "<circle cx=\"" << fmtNum(cx) << "\" cy=\"" << fmtNum(cy) << "\" r=\"" << fmtNum(r)
          << "\"" << paint("fill", color) << shadowFilter() << "/>\n";
    shadowColor = savedColor;
    shadowBlur = savedBlur;
}

void Svg::image(const std::string& dataUri, double x, double y, double w, double h) {
    body_ << "<image x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" width=\"" << fmtNum(w)
          << "\" height=\"" << fmtNum(h) << "\" preserveAspectRatio=\"none\" href=\"" << dataUri
          << "\"/>\n";
}

std::string Svg::str(const std::string& viewBox) const {
    std::ostringstream out;
    const std::string vb = viewBox.empty() ? "0 0 " + fmtNum(width_) + " " + fmtNum(height_) : viewBox;
    double vw = width_, vh = height_;
    if (!viewBox.empty()) std::sscanf(viewBox.c_str(), "%*f %*f %lf %lf", &vw, &vh);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << fmtNum(vw) << "\" height=\""
        << fmtNum(vh) << "\" viewBox=\"" << vb << "\">\n";
    out << "<defs>\n" << defs_.str() << "</defs>\n" << body_.str() << "</svg>\n";
    return out.str();
}

// minimal png writer (no compression)

namespace {
std::uint32_t crc32(const std::string& data) {
    static std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : data) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
void put32(std::string& s, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) s += static_cast<char>((v >> (8 * i)) & 0xFF);
}
void chunk(std::string& png, const std::string& type, const std::string& data) {
    put32(png, static_cast<std::uint32_t>(data.size()));
    const std::string td = type + data;
    png += td;
    put32(png, crc32(td));
}
}  // namespace

std::string pngDataUri(int width, int height, const std::vector<std::uint8_t>& rgba) {
    std::string raw;
    raw.reserve(static_cast<std::size_t>(height) * (width * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw += '\0';  // filter: none
        raw.append(reinterpret_cast<const char*>(&rgba[static_cast<std::size_t>(y) * width * 4]),
                   static_cast<std::size_t>(width) * 4);
    }
    std::string z = "\x78\x01";
    std::uint32_t a = 1, b = 0;
    for (unsigned char ch : raw) {
        a = (a + ch) % 65521;
        b = (b + a) % 65521;
    }
    for (std::size_t pos = 0; pos < raw.size() || pos == 0;) {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        z += static_cast<char>(last ? 1 : 0);
        z += static_cast<char>(len & 0xFF);
        z += static_cast<char>(len >> 8);
        z += static_cast<char>(~len & 0xFF);
        z += static_cast<char>((~len >> 8) & 0xFF);
        z.append(raw, pos, len);
        pos += len;
        if (last) break;
    }
    put32(z, (b << 16) | a);

    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    put32(ihdr, static_cast<std::uint32_t>(width));
    put32(ihdr, static_cast<std::uint32_t>(height));
    ihdr += std::string("\x08\x06\x00\x00\x00", 5);  // 8-bit RGBA, no interlace
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", "");
    return "data:image/png;base64," + base64(png);
}

}  // namespace kerr
