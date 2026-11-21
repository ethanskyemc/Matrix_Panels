#pragma once

#include <array>
#include <cstdint>
#include "config_defaults.h"
#include "gfx/types.h"

namespace gfx {

class Canvas {
public:
    static constexpr int W = kMatrixWidth;
    static constexpr int H = kMatrixHeight;
    static constexpr int N = kMatrixPixels;

    Canvas();

    void clear(RGB color = palette::Black);
    void fill(RGB color);
    void fadeToBlack(uint8_t amount); // amount 0..255 (higher = faster fade)
    void scaleBrightness(uint8_t brightness);

    inline bool inBounds(int x, int y) const {
        return x >= 0 && x < W && y >= 0 && y < H;
    }

    void setPixel(int x, int y, RGB color);
    void blendPixel(int x, int y, RGB color, uint8_t alpha);
    void addPixel(int x, int y, RGB color);
    RGB getPixel(int x, int y) const;

    void drawHLine(int x, int y, int len, RGB color);
    void drawVLine(int x, int y, int len, RGB color);
    void drawLine(int x0, int y0, int x1, int y1, RGB color);
    void drawRect(int x, int y, int w, int h, RGB color);
    void fillRect(int x, int y, int w, int h, RGB color);
    void drawCircle(int cx, int cy, int r, RGB color);
    void fillCircle(int cx, int cy, int r, RGB color);
    void drawProgressBar(int x, int y, int w, float progress, RGB fg, RGB bg);
    void drawGradientV(RGB top, RGB bottom);
    void drawGradientH(RGB left, RGB right);

    // Draw an arbitrary 16x16 packed RGB888 buffer (768 bytes)
    void copyFromRGB888(const uint8_t* rgb888);
    void copyToRGB888(uint8_t* rgb888) const;

    const RGB* data() const { return pixels_.data(); }
    RGB* data() { return pixels_.data(); }

private:
    std::array<RGB, N> pixels_;
};

} // namespace gfx
