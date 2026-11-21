#include "modes/all_modes.h"

#include <cmath>
#include <string>
#include "core/app_state.h"

namespace modes {

class SeasonalMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Seasonal; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        std::string theme = state.modeSettings.seasonalTheme;
        if (theme == "auto") {
            const auto& tm = state.live.time;
            if (tm.month == 12 && tm.day >= 1 && tm.day <= 26) theme = "christmas";
            else if ((tm.month == 12 && tm.day >= 27) || (tm.month == 1 && tm.day <= 3)) theme = "newyear";
            else if (tm.month == 2 && tm.day >= 10 && tm.day <= 16) theme = "valentine";
            else if (tm.month == 10 && tm.day >= 15) theme = "halloween";
            else if (tm.month >= 3 && tm.month <= 5) theme = "spring";
            else if (tm.month >= 6 && tm.month <= 8) theme = "summer";
            else if (tm.month >= 9 && tm.month <= 11) theme = "autumn";
            else theme = "winter";
        }

        if (theme == "halloween")      renderHalloween(c);
        else if (theme == "christmas") renderChristmas(c);
        else if (theme == "newyear")   renderNewYear(c);
        else if (theme == "valentine") renderValentine(c);
        else if (theme == "spring")    renderSpring(c);
        else if (theme == "summer")    renderSummer(c);
        else if (theme == "autumn")    renderAutumn(c);
        else                           renderWinter(c);
    }

private:
    float t_ = 0.0f;

    void renderChristmas(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(6, 10, 28), gfx::RGB(12, 20, 42));
        // Snow ground
        c.fillRect(0, 14, 16, 2, gfx::RGB(210, 228, 248));
        // Trunk
        c.fillRect(7, 12, 2, 2, gfx::RGB(120, 68, 30));
        // Pine tree triangle
        for (int row = 0; row < 9; ++row) {
            int half = row / 2 + 1;
            int y = 3 + row;
            c.drawHLine(8 - half, y, half * 2, gfx::RGB(18, 138 + row * 6, 48));
        }
        // Star topper
        float glow = 0.7f + 0.3f * std::sin(t_ * 5.0f);
        c.setPixel(7, 1, gfx::palette::Yellow.scaledF(glow));
        c.setPixel(8, 1, gfx::palette::Yellow.scaledF(glow));
        c.setPixel(7, 2, gfx::palette::WarmWhite.scaledF(glow));
        c.setPixel(8, 2, gfx::palette::WarmWhite.scaledF(glow));
        // Twinkling ornaments
        static constexpr int kOrns[6][2] = {{7,5},{9,6},{6,8},{9,9},{5,10},{10,11}};
        static constexpr gfx::RGB kOrnCols[6] = {
            gfx::palette::Red, gfx::palette::Gold, gfx::palette::Cyan,
            gfx::palette::Magenta, gfx::palette::Red, gfx::palette::Yellow
        };
        for (int i = 0; i < 6; ++i) {
            float tw = 0.45f + 0.55f * std::sin(t_ * 4.0f + i * 1.3f);
            c.setPixel(kOrns[i][0], kOrns[i][1], kOrnCols[i].scaledF(tw));
        }
        // Gentle falling snow
        for (int i = 0; i < 5; ++i) {
            int sx = (i * 3 + 1 + static_cast<int>(std::sin(t_ + i))) % 16;
            int sy = static_cast<int>(std::fmod(t_ * 2.2f + i * 3.1f, 14.0f));
            if (c.getPixel(sx, sy).g < 80) c.setPixel(sx, sy, gfx::RGB(210, 230, 255));
        }
    }

    void renderHalloween(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(20, 6, 36), gfx::RGB(8, 4, 18));
        // Stem
        c.fillRect(7, 1, 2, 2, gfx::RGB(46, 132, 38));
        // Pumpkin body
        c.fillCircle(7, 9, 5, gfx::palette::Orange);
        c.fillCircle(8, 9, 5, gfx::RGB(235, 110, 10));
        // Flickering candlelight inside carved eyes & mouth
        float flick = 0.72f + 0.28f * std::sin(t_ * 14.0f) * std::cos(t_ * 7.3f);
        gfx::RGB glow = gfx::palette::Yellow.scaledF(flick);
        // Eyes
        c.fillRect(5, 6, 2, 2, glow);
        c.fillRect(9, 6, 2, 2, glow);
        // Nose
        c.setPixel(7, 8, glow);
        c.setPixel(8, 8, glow);
        // Grinning mouth
        c.drawHLine(4, 10, 8, glow);
        c.setPixel(5, 11, glow);
        c.setPixel(7, 11, glow);
        c.setPixel(8, 11, glow);
        c.setPixel(10, 11, glow);
    }

    void renderNewYear(gfx::Canvas& c) {
        c.clear(gfx::RGB(4, 6, 18));
        // 3 staggered bursting fireworks
        for (int b = 0; b < 3; ++b) {
            float cycle = std::fmod(t_ * 0.75f + b * 0.33f, 1.0f);
            int cx = 4 + b * 4;
            int cy = 4 + (b % 2) * 4;
            float hue = std::fmod(b * 120.0f + t_ * 30.0f, 360.0f);
            if (cycle < 0.25f) {
                // Rising rocket
                int ry = 15 - static_cast<int>((cycle / 0.25f) * (15 - cy));
                c.setPixel(cx, ry, gfx::palette::WarmWhite);
                c.setPixel(cx, ry + 1, gfx::palette::Amber);
            } else {
                // Burst sparks
                float p = (cycle - 0.25f) / 0.75f;
                float r = p * 5.8f;
                float fade = 1.0f - p;
                for (int i = 0; i < 10; ++i) {
                    float a = i * (2.0f * 3.14159f / 10.0f);
                    int sx = static_cast<int>(std::round(cx + std::cos(a) * r));
                    int sy = static_cast<int>(std::round(cy + std::sin(a) * r + p * p * 2.5f));
                    c.addPixel(sx, sy, gfx::hsvToRgb(hue + i * 18.0f, 0.9f, fade));
                }
            }
        }
    }

    void renderValentine(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(24, 4, 18), gfx::RGB(12, 2, 12));
        float beat = 1.0f + 0.14f * std::pow(std::max(0.0f, std::sin(t_ * 4.2f)), 4.0f);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float nx = (x - 7.5f) / (5.5f * beat);
                float ny = -(y - 8.2f) / (5.5f * beat);
                float eq = std::pow(nx * nx + ny * ny - 1.0f, 3.0f) - nx * nx * ny * ny * ny;
                if (eq <= 0.0f) {
                    float shade = 0.75f + 0.25f * (1.0f - std::min(1.0f, std::sqrt(nx * nx + ny * ny)));
                    c.setPixel(x, y, gfx::lerpColor(gfx::palette::Red, gfx::palette::Pink, shade * 0.45f));
                }
            }
        }
    }

    void renderSpring(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(24, 62, 110), gfx::RGB(44, 112, 82));
        // Cherry blossom branch
        c.drawLine(0, 5, 9, 3, gfx::RGB(96, 58, 36));
        c.drawLine(5, 4, 12, 6, gfx::RGB(96, 58, 36));
        c.fillCircle(4, 3, 2, gfx::palette::Pink);
        c.fillCircle(9, 3, 2, gfx::RGB(255, 168, 204));
        c.fillCircle(12, 5, 1, gfx::palette::Pink);
        // Drifting petals
        for (int i = 0; i < 6; ++i) {
            float fy = std::fmod(t_ * 2.4f + i * 2.6f, 13.0f) + 3.0f;
            int px = static_cast<int>(std::fmod(i * 3.0f + t_ * 1.4f + std::sin(t_ * 2.0f + i) * 1.5f, 16.0f));
            if (px < 0) px += 16;
            c.setPixel(px, static_cast<int>(fy), (i % 2 == 0) ? gfx::palette::Pink : gfx::palette::WarmWhite);
        }
    }

    void renderSummer(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(20, 120, 220), gfx::RGB(255, 165, 70));
        // Glowing sun
        c.fillCircle(12, 3, 2, gfx::palette::Yellow);
        c.setPixel(12, 3, gfx::palette::White);
        // Shimmering ocean & beach
        for (int x = 0; x < 16; ++x) {
            int waveY = 9 + static_cast<int>(std::round(std::sin(t_ * 3.0f + x * 0.6f) * 0.7f));
            for (int y = waveY; y < 13; ++y) {
                c.setPixel(x, y, (y == waveY) ? gfx::palette::Cyan : gfx::RGB(16, 110, 195));
            }
            for (int y = 13; y < 16; ++y) {
                c.setPixel(x, y, gfx::RGB(235, 194, 110));
            }
        }
    }

    void renderAutumn(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(28, 16, 42), gfx::RGB(74, 34, 22));
        // Leaf pile at bottom
        for (int x = 0; x < 16; ++x) {
            c.setPixel(x, 15, (x % 3 == 0) ? gfx::palette::Red : (x % 3 == 1) ? gfx::palette::Orange : gfx::palette::Amber);
            if ((x % 2) == 0) c.setPixel(x, 14, gfx::palette::Orange);
        }
        // Falling autumn leaves
        static constexpr gfx::RGB kLeafCols[4] = {
            gfx::palette::Red, gfx::palette::Orange, gfx::palette::Amber, gfx::palette::Gold
        };
        for (int i = 0; i < 8; ++i) {
            float fy = std::fmod(t_ * (1.8f + (i % 3) * 0.4f) + i * 2.1f, 14.5f);
            float fx = std::fmod(i * 2.3f + std::sin(t_ * 2.2f + i * 1.4f) * 1.8f + 16.0f, 16.0f);
            c.setPixel(static_cast<int>(fx), static_cast<int>(fy), kLeafCols[i % 4]);
        }
    }

    void renderWinter(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(10, 20, 48), gfx::RGB(24, 48, 86));
        c.fillRect(0, 14, 16, 2, gfx::RGB(220, 236, 255));
        for (int i = 0; i < 12; ++i) {
            float fy = std::fmod(t_ * (1.6f + (i % 4) * 0.45f) + i * 1.7f, 14.0f);
            int fx = static_cast<int>(std::fmod(i * 1.9f + std::sin(t_ * 1.5f + i) * 1.2f + 16.0f, 16.0f));
            c.setPixel(fx, static_cast<int>(fy), (i % 3 == 0) ? gfx::palette::White : gfx::RGB(175, 210, 255));
        }
    }
};

std::unique_ptr<DisplayMode> createSeasonalMode() {
    return std::make_unique<SeasonalMode>();
}

} // namespace modes
