#include "modes/all_modes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class HeartRateMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::HeartRate; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        float bpm = std::clamp(state.live.heartRateBpm, 40.0f, 190.0f);
        float beatsPerSec = bpm / 60.0f;
        beatPhase_ = std::fmod(beatPhase_ + dtSec * beatsPerSec, 1.0f);
        t_ += dtSec;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        c.clear(gfx::RGB(14, 4, 8));

        if (!state.live.vitalPresent || !state.live.vitalValid) {
            // Scanning pulse when awaiting mmWave target
            float p = 0.5f + 0.5f * std::sin(t_ * 3.0f);
            drawSmallHeart(c, 5, 2, gfx::palette::Red.scaledF(0.35f + 0.45f * p), false);
            gfx::drawTextCentered3x5(c, 10, "---", gfx::palette::Gray, 1);
            return;
        }

        // Lub-dub double systolic pulse envelope synced to real BPM
        float lub = std::exp(-std::pow((beatPhase_ - 0.14f) / 0.07f, 2.0f));
        float dub = 0.65f * std::exp(-std::pow((beatPhase_ - 0.30f) / 0.08f, 2.0f));
        float pulse = std::clamp(lub + dub, 0.0f, 1.0f);

        // Background ECG trace across upper half when enabled
        if (state.modeSettings.heartRateShowEcg) {
            for (int x = 0; x < 16; ++x) {
                float u = std::fmod(beatPhase_ - static_cast<float>(x) / 16.0f + 2.0f, 1.0f);
                int ecgY = 6;
                if (u > 0.10f && u < 0.16f)      ecgY = 2; // R spike
                else if (u >= 0.16f && u < 0.21f) ecgY = 8; // S dip
                else if (u > 0.28f && u < 0.36f)  ecgY = 5; // T wave
                float trail = 1.0f - static_cast<float>(x) / 16.0f;
                c.blendPixel(x, ecgY, gfx::RGB(255, 64, 96), static_cast<uint8_t>(trail * 110.0f));
            }
        }

        // Pulsing pixel heart at top-center
        gfx::RGB heartCol = gfx::lerpColor(gfx::RGB(190, 22, 48), gfx::RGB(255, 72, 118), pulse);
        drawSmallHeart(c, 4, 1, heartCol, pulse > 0.45f);

        // Crisp BPM digits (y=10..14)
        int bpmInt = static_cast<int>(std::round(std::clamp(state.live.heartRateBpm, 0.0f, 240.0f)));
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", bpmInt);
        gfx::drawTextCentered3x5(c, 10, std::string(buf), gfx::palette::WarmWhite, 1);
    }

private:
    float beatPhase_ = 0.0f;
    float t_ = 0.0f;

    void drawSmallHeart(gfx::Canvas& c, int ox, int oy, gfx::RGB col, bool expanded) {
        static constexpr uint8_t kHeart7x6[6] = {
            0b0110110,
            0b1111111,
            0b1111111,
            0b0111110,
            0b0011100,
            0b0001000
        };
        for (int r = 0; r < 6; ++r) {
            for (int colIdx = 0; colIdx < 7; ++colIdx) {
                if (kHeart7x6[r] & (1 << (6 - colIdx))) {
                    c.setPixel(ox + colIdx, oy + r, col);
                }
            }
        }
        if (expanded) {
            c.blendPixel(ox + 1, oy - 1, col, 130);
            c.blendPixel(ox + 5, oy - 1, col, 130);
            c.blendPixel(ox - 1, oy + 2, col, 130);
            c.blendPixel(ox + 7, oy + 2, col, 130);
        }
    }
};

std::unique_ptr<DisplayMode> createHeartRateMode() {
    return std::make_unique<HeartRateMode>();
}

} // namespace modes
