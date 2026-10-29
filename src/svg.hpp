// simple svg writer with a canvas-like api
#pragma once

#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace kerr {

struct Rgba {
    int r = 0, g = 0, b = 0;
    double a = 1.0;
};
Rgba parseColor(const std::string& css);

class Svg {
public:
    Svg(double width, double height);

    std::string fillStyle = "#000000";
    std::string strokeStyle = "#000000";
    double lineWidth = 1.0;
    std::string shadowColor = "#00000000";
    double shadowBlur = 0;
    double fontSize = 10;
    int fontWeight = 400;
    std::string textAlign = "start";
    void setLineDash(std::vector<double> d) { dash_ = std::move(d); }

    void save();
    void restore();
    void translate(double x, double y);
    void rotate(double radians);

    void beginPath();
    void moveTo(double x, double y);
    void lineTo(double x, double y);
    void arc(double cx, double cy, double r);  // full circle
    void stroke();
    void fill();
    void fillRect(double x, double y, double w, double h);
    void fillText(const std::string& text, double x, double y);

    // clipped region with its own origin
    void beginCanvas(double x, double y, double w, double h);
    void endCanvas();

    void rect(double x, double y, double w, double h, const std::string& fill, double radius = 0);
    void strokeRectInset(double x, double y, double w, double h, const std::string& color,
                         double radius = 0);
    void text(const std::string& s, double x, double baseline, double size, int weight,
              const std::string& color, double letterSpacing = 0);
    void circleGlow(double cx, double cy, double r, const std::string& color, double blur);
    void image(const std::string& dataUri, double x, double y, double w, double h);

    void embedFonts(const std::string& fontDir);

    std::string str(const std::string& viewBox = "") const;

private:
    struct State {
        std::string fillStyle, strokeStyle, shadowColor, textAlign;
        double lineWidth, shadowBlur, fontSize;
        int fontWeight;
        std::vector<double> dash;
        std::string transform;
    };
    std::string paint(const std::string& attr, const std::string& css) const;
    std::string shadowFilter();
    std::string transformAttr() const;

    double width_, height_;
    std::ostringstream body_;
    std::ostringstream defs_;
    std::string fontCss_;
    std::vector<double> dash_;
    std::string path_;
    std::string transform_;
    std::vector<State> stack_;
    std::map<std::string, std::string> filters_;
    int nextId_ = 0;
    double canvasW_ = 0, canvasH_ = 0;
};

std::string pngDataUri(int width, int height, const std::vector<std::uint8_t>& rgba);

std::string fmtNum(double v);

}  // namespace kerr
