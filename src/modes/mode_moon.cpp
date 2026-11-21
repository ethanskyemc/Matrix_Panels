#include "modes/all_modes.h"

#include <cmath>
#include <cstdio>
#include <string>
#include "core/app_state.h"
#include "core/astro.h"
#include "gfx/font.h"

namespace modes {

class MoonMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::MoonPhase; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        c.drawGradientV(gfx::RGB(4, 6, 20), gfx::RGB(8, 12, 30));

        // Twinkling stars in corners
        static constexpr int kStars[8][2] = {
            {1, 1}, {14, 2}, {2, 13}, {14, 14}, {0, 7}, {15, 8}, {12, 0}, {1, 10}
        };
        for (int i = 0; i < 8; ++i) {
            float tw = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t_ * (1.8f + i * 0.3f) + i * 1.7f));
            c.setPixel(kStars[i][0], kStars[i][1], gfx::RGB(160, 185, 255).scaledF(tw * 0.7f));
        }

        core::MoonInfo moon = core::computeMoonPhase(state.live.time.unixEpoch);
        float phaseAngle = static_cast<float>(moon.phase * 2.0 * 3.1415926535);
        // Sun direction vector in lunar disc coordinates
        float sx = std::sin(phaseAngle);
        float sz = -std::cos(phaseAngle);

        float cx = 7.5f;
        float cy = state.modeSettings.moonShowPercent ? 5.2f : 7.5f;
        float radius = state.modeSettings.moonShowPercent ? 4.4f : 5.6f;

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float dx = (x - cx) / radius;
                float dy = (y - cy) / radius;
                float r2 = dx * dx + dy * dy;
                if (r2 > 1.0f) continue;

                float dz = std::sqrt(std::max(0.0f, 1.0f - r2));
                float ndotl = dx * sx + dz * sz;

                // Subtle surface crater variation
                float crater = 0.90f + 0.10f * std::sin(x * 1.7f + y * 0.9f) * std::cos(y * 1.3f - x * 0.6f);

                // Smooth terminator anti-aliasing
                float lit = std::clamp((ndotl + 0.08f) / 0.22f, 0.0f, 1.0f);
                // Soft earthshine on dark side so lunar disc silhouette remains clear behind acrylic
                gfx::RGB darkSide = gfx::RGB(18, 24, 46);
                gfx::RGB litSide = gfx::RGB(248, 240, 210).scaledF(crater * (0.72f + 0.28f * dz));

                // Soft limb edge anti-aliasing
                float edge = std::clamp((1.0f - std::sqrt(r2)) * 4.0f, 0.0f, 1.0f);
                gfx::RGB col = gfx::lerpColor(darkSide, litSide, lit);
                c.blendPixel(x, y, col, static_cast<uint8_t>(edge * 255.0f));
            }
        }

        if (state.modeSettings.moonShowPercent) {
            int pct = static_cast<int>(std::round(moon.illumination * 100.0));
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d%%", pct);
            gfx::drawTextCentered3x5(c, 11, std::string(buf), gfx::palette::WarmWhite, 1);
        }
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createMoonMode() {
    return std::make_unique<MoonMode>();
}

} // namespace modes
