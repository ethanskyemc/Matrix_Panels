#include "gfx/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gfx {

Canvas::Canvas() {
    clear();
}

void Canvas::clear(RGB color) {
    pixels_.fill(color);
}

void Canvas::fill(RGB color) {
    pixels_.fill(color);
}

void Canvas::fadeToBlack(uint8_t amount) {
    uint8_t scale = static_cast<uint8_t>(255 - amount);
    for (auto& p : pixels_) {
        p = p.scaled(scale);
    }
}

void Canvas::scaleBrightness(uint8_t brightness) {
    if (brightness == 255) return;
    for (auto& p : pixels_) {
        p = p.scaled(brightness);
    }
}

void Canvas::setPixel(int x, int y, RGB color) {
    if (!inBounds(x, y)) return;
    pixels_[y * W + x] = color;
}

void Canvas::blendPixel(int x, int y, RGB color, uint8_t alpha) {
    if (!inBounds(x, y)) return;
    auto& dst = pixels_[y * W + x];
    dst = blendAlpha(dst, color, alpha);
}

void Canvas::addPixel(int x, int y, RGB color) {
    if (!inBounds(x, y)) return;
    auto& dst = pixels_[y * W + x];
    dst = dst.added(color);
}

RGB Canvas::getPixel(int x, int y) const {
    if (!inBounds(x, y)) return palette::Black;
    return pixels_[y * W + x];
}

void Canvas::drawHLine(int x, int y, int len, RGB color) {
    for (int i = 0; i < len; ++i) {
        setPixel(x + i, y, color);
    }
}

void Canvas::drawVLine(int x, int y, int len, RGB color) {
    for (int i = 0; i < len; ++i) {
        setPixel(x, y + i, color);
    }
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, RGB color) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::drawRect(int x, int y, int w, int h, RGB color) {
    if (w <= 0 || h <= 0) return;
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
}

void Canvas::fillRect(int x, int y, int w, int h, RGB color) {
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            setPixel(xx, yy, color);
        }
    }
}

void Canvas::drawCircle(int cx, int cy, int r, RGB color) {
    int x = r;
    int y = 0;
    int err = 1 - x;
    while (x >= y) {
        setPixel(cx + x, cy + y, color);
        setPixel(cx + y, cy + x, color);
        setPixel(cx - y, cy + x, color);
        setPixel(cx - x, cy + y, color);
        setPixel(cx - x, cy - y, color);
        setPixel(cx - y, cy - x, color);
        setPixel(cx + y, cy - x, color);
        setPixel(cx + x, cy - y, color);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x + 1);
        }
    }
}

void Canvas::fillCircle(int cx, int cy, int r, RGB color) {
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                setPixel(cx + x, cy + y, color);
            }
        }
    }
}

void Canvas::drawProgressBar(int x, int y, int w, float progress, RGB fg, RGB bg) {
    float p = std::clamp(progress, 0.0f, 1.0f);
    float filled = p * static_cast<float>(w);
    int full = static_cast<int>(filled);
    float frac = filled - static_cast<float>(full);

    for (int i = 0; i < w; ++i) {
        if (i < full) {
            setPixel(x + i, y, fg);
        } else if (i == full && frac > 0.05f) {
            setPixel(x + i, y, lerpColor(bg, fg, frac));
        } else {
            setPixel(x + i, y, bg);
        }
    }
}

void Canvas::drawGradientV(RGB top, RGB bottom) {
    for (int y = 0; y < H; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(H - 1);
        RGB c = lerpColor(top, bottom, t);
        drawHLine(0, y, W, c);
    }
}

void Canvas::drawGradientH(RGB left, RGB right) {
    for (int x = 0; x < W; ++x) {
        float t = static_cast<float>(x) / static_cast<float>(W - 1);
        RGB c = lerpColor(left, right, t);
        drawVLine(x, 0, H, c);
    }
}

void Canvas::copyFromRGB888(const uint8_t* rgb888) {
    if (!rgb888) return;
    for (int i = 0; i < N; ++i) {
        pixels_[i] = RGB(rgb888[i * 3], rgb888[i * 3 + 1], rgb888[i * 3 + 2]);
    }
}

void Canvas::copyToRGB888(uint8_t* rgb888) const {
    if (!rgb888) return;
    for (int i = 0; i < N; ++i) {
        rgb888[i * 3 + 0] = pixels_[i].r;
        rgb888[i * 3 + 1] = pixels_[i].g;
        rgb888[i * 3 + 2] = pixels_[i].b;
    }
}

} // namespace gfx
