#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gfx {

struct RGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr RGB() = default;
    constexpr RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    static constexpr RGB fromHex(uint32_t hex) {
        return RGB(
            static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF),
            static_cast<uint8_t>(hex & 0xFF)
        );
    }

    constexpr uint32_t toHex() const {
        return (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    RGB scaled(uint8_t scale) const {
        return RGB(
            static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) / 255)
        );
    }

    RGB scaledF(float factor) const {
        float f = std::clamp(factor, 0.0f, 1.0f);
        return RGB(
            static_cast<uint8_t>(r * f + 0.5f),
            static_cast<uint8_t>(g * f + 0.5f),
            static_cast<uint8_t>(b * f + 0.5f)
        );
    }

    RGB added(const RGB& o) const {
        return RGB(
            static_cast<uint8_t>(std::min<int>(255, r + o.r)),
            static_cast<uint8_t>(std::min<int>(255, g + o.g)),
            static_cast<uint8_t>(std::min<int>(255, b + o.b))
        );
    }

    bool operator==(const RGB& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const RGB& o) const { return !(*this == o); }
};

inline RGB lerpColor(const RGB& a, const RGB& b, float t) {
    float c = std::clamp(t, 0.0f, 1.0f);
    return RGB(
        static_cast<uint8_t>(a.r + (b.r - a.r) * c + 0.5f),
        static_cast<uint8_t>(a.g + (b.g - a.g) * c + 0.5f),
        static_cast<uint8_t>(a.b + (b.b - a.b) * c + 0.5f)
    );
}

inline RGB blendAlpha(const RGB& dst, const RGB& src, uint8_t alpha) {
    uint16_t inv = 255 - alpha;
    return RGB(
        static_cast<uint8_t>((src.r * alpha + dst.r * inv) / 255),
        static_cast<uint8_t>((src.g * alpha + dst.g * inv) / 255),
        static_cast<uint8_t>((src.b * alpha + dst.b * inv) / 255)
    );
}

// Hue in [0, 360), Saturation [0, 1], Value [0, 1]
inline RGB hsvToRgb(float h, float s, float v) {
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float rf = 0, gf = 0, bf = 0;
    if (h < 60.0f)       { rf = c; gf = x; bf = 0; }
    else if (h < 120.0f) { rf = x; gf = c; bf = 0; }
    else if (h < 180.0f) { rf = 0; gf = c; bf = x; }
    else if (h < 240.0f) { rf = 0; gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; gf = 0; bf = c; }
    else                 { rf = c; gf = 0; bf = x; }

    return RGB(
        static_cast<uint8_t>((rf + m) * 255.0f + 0.5f),
        static_cast<uint8_t>((gf + m) * 255.0f + 0.5f),
        static_cast<uint8_t>((bf + m) * 255.0f + 0.5f)
    );
}

// Perceptual gamma LUT tuned for WS2812B LEDs behind frosted acrylic
inline uint8_t gammaCorrect8(uint8_t v) {
    if (v == 0) return 0;
    float norm = static_cast<float>(v) / 255.0f;
    int out = static_cast<int>(std::pow(norm, 2.2f) * 255.0f + 0.5f);
    return static_cast<uint8_t>(std::clamp(out, 1, 255));
}

// Common acrylic-optimized palette constants
namespace palette {
    static constexpr RGB Black      = RGB(0, 0, 0);
    static constexpr RGB White      = RGB(255, 255, 255);
    static constexpr RGB WarmWhite  = RGB(255, 228, 185);
    static constexpr RGB Red        = RGB(255, 42, 42);
    static constexpr RGB Coral      = RGB(255, 96, 72);
    static constexpr RGB Orange     = RGB(255, 136, 16);
    static constexpr RGB Amber      = RGB(255, 188, 24);
    static constexpr RGB Yellow     = RGB(255, 232, 40);
    static constexpr RGB Lime       = RGB(132, 240, 48);
    static constexpr RGB Green      = RGB(32, 224, 96);
    static constexpr RGB Teal       = RGB(18, 212, 188);
    static constexpr RGB Cyan       = RGB(36, 216, 255);
    static constexpr RGB SkyBlue    = RGB(74, 174, 255);
    static constexpr RGB DeepBlue   = RGB(18, 58, 196);
    static constexpr RGB Midnight   = RGB(8, 14, 44);
    static constexpr RGB Indigo     = RGB(86, 62, 255);
    static constexpr RGB Purple     = RGB(174, 58, 255);
    static constexpr RGB Magenta    = RGB(255, 48, 178);
    static constexpr RGB Pink       = RGB(255, 118, 172);
    static constexpr RGB Gold       = RGB(255, 206, 56);
    static constexpr RGB Gray       = RGB(110, 120, 134);
    static constexpr RGB DimGray    = RGB(42, 48, 58);
}

} // namespace gfx
