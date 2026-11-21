#include "modes/all_modes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "core/app_state.h"

namespace modes {

// ---- 1. FIREPLACE ----
class FireplaceMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Fireplace; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
        accum_ += dtSec * state.animationSpeed;
        while (accum_ >= 0.045f) {
            accum_ -= 0.045f;
            stepFire();
        }
    }

    void render(gfx::Canvas& c, const core::AppState&, const core::MediaStore&) override {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float h = heat_[y * 16 + x];
                c.setPixel(x, y, heatColor(h));
            }
        }
        // Dark charred wood logs at bottom with glowing ember cracks
        c.fillRect(2, 14, 12, 2, gfx::RGB(56, 26, 12));
        c.setPixel(4, 14, gfx::RGB(210, 74, 14));
        c.setPixel(8, 15, gfx::RGB(235, 92, 18));
        c.setPixel(11, 14, gfx::RGB(200, 64, 10));
    }

private:
    float t_ = 0.0f;
    float accum_ = 0.0f;
    uint32_t rng_ = 0x13579BDFu;
    std::array<float, 256> heat_{};

    float rand01() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>((rng_ >> 8) & 0xFFFFu) / 65535.0f;
    }

    void stepFire() {
        // Cool & propagate upward
        for (int y = 0; y < 15; ++y) {
            for (int x = 0; x < 16; ++x) {
                int xl = std::max(0, x - 1);
                int xr = std::min(15, x + 1);
                float below = (heat_[(y + 1) * 16 + xl] +
                               heat_[(y + 1) * 16 + x] * 2.0f +
                               heat_[(y + 1) * 16 + xr]) * 0.25f;
                // Slightly taller flame in center than edges
                float edgeCool = std::fabs(x - 7.5f) * 0.015f;
                float cool = 0.045f + rand01() * 0.085f + edgeCool;
                heat_[y * 16 + x] = std::max(0.0f, below - cool);
            }
        }
        // Fuel base row
        for (int x = 0; x < 16; ++x) {
            float centerWeight = 1.0f - std::pow((x - 7.5f) / 8.5f, 2.0f);
            heat_[15 * 16 + x] = std::clamp(centerWeight * (0.72f + rand01() * 0.45f), 0.1f, 1.0f);
        }
        // Occasional rising spark
        if (rand01() < 0.28f) {
            int sx = 3 + static_cast<int>(rand01() * 10.0f);
            int sy = 2 + static_cast<int>(rand01() * 5.0f);
            heat_[sy * 16 + sx] = std::max(heat_[sy * 16 + sx], 0.85f);
        }
    }

    static gfx::RGB heatColor(float h) {
        h = std::clamp(h, 0.0f, 1.0f);
        if (h < 0.25f) return gfx::lerpColor(gfx::RGB(4, 2, 2), gfx::RGB(145, 16, 4), h / 0.25f);
        if (h < 0.58f) return gfx::lerpColor(gfx::RGB(145, 16, 4), gfx::RGB(255, 108, 8), (h - 0.25f) / 0.33f);
        if (h < 0.85f) return gfx::lerpColor(gfx::RGB(255, 108, 8), gfx::RGB(255, 220, 42), (h - 0.58f) / 0.27f);
        return gfx::lerpColor(gfx::RGB(255, 220, 42), gfx::RGB(255, 250, 215), (h - 0.85f) / 0.15f);
    }
};

// ---- 2. OCEAN WAVES ----
class WavesMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Waves; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState&, const core::MediaStore&) override {
        // Moonlit twilight sky
        c.drawGradientV(gfx::RGB(8, 16, 48), gfx::RGB(22, 46, 86));
        c.fillCircle(12, 2, 1, gfx::palette::WarmWhite);

        // 3 layered parallax ocean waves with sparkling seafoam
        struct WaveLayer { float baseY; float amp; float freq; float speed; gfx::RGB body; gfx::RGB crest; };
        const WaveLayer layers[3] = {
            {6.8f,  1.1f, 0.42f,  1.4f, gfx::RGB(14, 62, 140),  gfx::RGB(90, 175, 235)},
            {9.5f,  1.3f, 0.52f, -2.1f, gfx::RGB(10, 96, 168),  gfx::RGB(155, 228, 250)},
            {12.4f, 1.2f, 0.62f,  2.8f, gfx::RGB(12, 134, 176), gfx::RGB(232, 248, 255)},
        };

        for (int l = 0; l < 3; ++l) {
            const auto& w = layers[l];
            for (int x = 0; x < 16; ++x) {
                float wy = w.baseY + std::sin(x * w.freq + t_ * w.speed + l * 1.7f) * w.amp;
                int topY = static_cast<int>(std::round(wy));
                for (int y = std::max(0, topY); y < 16; ++y) {
                    if (y == topY) {
                        float sparkle = 0.55f + 0.45f * std::sin(t_ * 4.5f + x * 1.3f + l);
                        c.setPixel(x, y, gfx::lerpColor(w.body, w.crest, sparkle));
                    } else {
                        float depth = std::clamp(static_cast<float>(y - topY) / 5.0f, 0.0f, 1.0f);
                        c.setPixel(x, y, gfx::lerpColor(w.body, gfx::RGB(6, 28, 68), depth * 0.6f));
                    }
                }
            }
        }
    }

private:
    float t_ = 0.0f;
};

// ---- 3. AQUARIUM ----
class AquariumMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Aquarium; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState&, const core::MediaStore&) override {
        // Deep aqua water gradient
        c.drawGradientV(gfx::RGB(8, 52, 108), gfx::RGB(4, 24, 58));

        // Subtle surface water shimmer
        for (int x = 0; x < 16; ++x) {
            float sh = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(t_ * 3.0f + x * 0.7f));
            c.setPixel(x, 0, gfx::RGB(42, 168, 224).scaledF(sh));
        }

        // Sandy floor + pebbles
        c.fillRect(0, 14, 16, 2, gfx::RGB(196, 164, 98));
        c.setPixel(3, 14, gfx::RGB(145, 118, 72));
        c.setPixel(10, 15, gfx::RGB(145, 118, 72));

        // Swaying kelp stalks
        const int kelpX[3] = {2, 12, 14};
        const int kelpH[3] = {7, 6, 5};
        for (int k = 0; k < 3; ++k) {
            for (int seg = 0; seg < kelpH[k]; ++seg) {
                int y = 13 - seg;
                int sway = static_cast<int>(std::round(std::sin(t_ * 2.2f + seg * 0.6f + k) * (seg > 2 ? 1.0f : 0.3f)));
                c.setPixel(kelpX[k] + sway, y, (seg % 2 == 0) ? gfx::RGB(24, 168, 72) : gfx::RGB(36, 205, 92));
            }
        }

        // Rising air bubbles
        for (int b = 0; b < 4; ++b) {
            float by = 13.0f - std::fmod(t_ * (2.6f + b * 0.4f) + b * 3.3f, 13.0f);
            int bx = 3 + b * 3 + static_cast<int>(std::round(std::sin(t_ * 3.0f + b) * 0.5f));
            c.blendPixel(bx, static_cast<int>(by), gfx::RGB(190, 240, 255), 180);
        }

        // Swimming tropical fish 1 (Clownfish moving right)
        float f1x = std::fmod(t_ * 3.2f, 22.0f) - 3.0f;
        int f1y = 5 + static_cast<int>(std::round(std::sin(t_ * 2.0f) * 1.2f));
        drawFish(c, static_cast<int>(f1x), f1y, true, gfx::palette::Orange, gfx::palette::White);

        // Swimming tropical fish 2 (Blue tang moving left)
        float f2x = 18.0f - std::fmod(t_ * 2.5f + 9.0f, 22.0f);
        int f2y = 10 + static_cast<int>(std::round(std::cos(t_ * 1.8f) * 1.0f));
        drawFish(c, static_cast<int>(f2x), f2y, false, gfx::palette::Cyan, gfx::palette::Yellow);
    }

private:
    float t_ = 0.0f;

    void drawFish(gfx::Canvas& c, int x, int y, bool facingRight, gfx::RGB body, gfx::RGB stripe) {
        int dir = facingRight ? 1 : -1;
        c.setPixel(x, y, body);
        c.setPixel(x + dir, y, stripe);
        c.setPixel(x + 2 * dir, y, body);
        c.setPixel(x + dir, y - 1, body);
        c.setPixel(x + dir, y + 1, body);
        // Wagging tail
        int wag = (static_cast<int>(t_ * 8.0f) % 2 == 0) ? 1 : -1;
        c.setPixel(x - dir, y + (wag > 0 ? -1 : 0), stripe);
        c.setPixel(x - dir, y + (wag > 0 ? 1 : 0), stripe);
    }
};

std::unique_ptr<DisplayMode> createFireplaceMode() {
    return std::make_unique<FireplaceMode>();
}
std::unique_ptr<DisplayMode> createWavesMode() {
    return std::make_unique<WavesMode>();
}
std::unique_ptr<DisplayMode> createAquariumMode() {
    return std::make_unique<AquariumMode>();
}

} // namespace modes
