#include "modes/all_modes.h"

#include <algorithm>
#include <cmath>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class MirrorMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Mirror; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        if (!state.live.mirrorFrameValid) {
            // Camera standby animated viewfinder silhouette
            c.drawGradientV(gfx::RGB(10, 14, 28), gfx::RGB(18, 22, 42));
            float scanY = std::fmod(t_ * 6.0f, 16.0f);
            c.drawHLine(0, static_cast<int>(scanY), 16, gfx::RGB(38, 120, 180));
            // Viewfinder brackets
            c.drawHLine(2, 2, 3, gfx::palette::Cyan); c.drawVLine(2, 2, 3, gfx::palette::Cyan);
            c.drawHLine(11, 2, 3, gfx::palette::Cyan); c.drawVLine(13, 2, 3, gfx::palette::Cyan);
            c.drawHLine(2, 13, 3, gfx::palette::Cyan); c.drawVLine(2, 11, 3, gfx::palette::Cyan);
            c.drawHLine(11, 13, 3, gfx::palette::Cyan); c.drawVLine(13, 11, 3, gfx::palette::Cyan);
            // Lens icon
            c.drawCircle(7, 7, 3, gfx::palette::WarmWhite);
            c.fillCircle(7, 7, 1, gfx::palette::SkyBlue);
            return;
        }

        const auto& pal = state.modeSettings.mirrorPalette;
        bool flipX = state.modeSettings.mirrorFlipX;

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                int sx = flipX ? (15 - x) : x;
                int idx = (y * 16 + sx) * 3;
                uint8_t r = state.live.mirrorRgb[idx + 0];
                uint8_t g = state.live.mirrorRgb[idx + 1];
                uint8_t b = state.live.mirrorRgb[idx + 2];

                float luma = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;

                if (pal == "thermal") {
                    c.setPixel(x, y, thermalPalette(luma));
                } else if (pal == "retro") {
                    // Classic 4-shade green handheld LCD palette
                    if (luma < 0.25f)      c.setPixel(x, y, gfx::RGB(15, 56, 15));
                    else if (luma < 0.50f) c.setPixel(x, y, gfx::RGB(48, 98, 48));
                    else if (luma < 0.75f) c.setPixel(x, y, gfx::RGB(139, 172, 15));
                    else                   c.setPixel(x, y, gfx::RGB(155, 188, 15));
                } else if (pal == "neon") {
                    float hue = std::fmod(270.0f - luma * 140.0f, 360.0f);
                    c.setPixel(x, y, gfx::hsvToRgb(hue, 0.92f, std::pow(luma, 0.85f)));
                } else {
                    c.setPixel(x, y, gfx::RGB(r, g, b));
                }
            }
        }
    }

private:
    float t_ = 0.0f;

    static gfx::RGB thermalPalette(float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        if (v < 0.25f) return gfx::lerpColor(gfx::RGB(8, 6, 48), gfx::RGB(115, 16, 168), v / 0.25f);
        if (v < 0.55f) return gfx::lerpColor(gfx::RGB(115, 16, 168), gfx::RGB(238, 48, 36), (v - 0.25f) / 0.30f);
        if (v < 0.82f) return gfx::lerpColor(gfx::RGB(238, 48, 36), gfx::RGB(255, 212, 30), (v - 0.55f) / 0.27f);
        return gfx::lerpColor(gfx::RGB(255, 212, 30), gfx::RGB(255, 255, 245), (v - 0.82f) / 0.18f);
    }
};

std::unique_ptr<DisplayMode> createMirrorMode() {
    return std::make_unique<MirrorMode>();
}

} // namespace modes
