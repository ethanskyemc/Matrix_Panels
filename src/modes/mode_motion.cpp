#include "modes/all_modes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "core/app_state.h"

namespace modes {

class MotionMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::MotionReactive; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
        float targetEnergy = state.live.motionDetected ? std::max(0.65f, state.live.motionIntensity) : 0.0f;
        if (targetEnergy > energy_) {
            energy_ += (targetEnergy - energy_) * std::min(1.0f, dtSec * 8.0f);
        } else {
            energy_ += (targetEnergy - energy_) * std::min(1.0f, dtSec * 1.6f);
        }

        // Spawn expanding ripple ring whenever motion triggers or pulses
        if (state.live.motionDetected && (!prevMotion_ || rippleCooldown_ <= 0.0f)) {
            ripples_[nextRipple_] = {
                std::clamp(state.live.motionX, 0.0f, 1.0f) * 15.0f,
                std::clamp(state.live.motionY, 0.0f, 1.0f) * 15.0f,
                0.2f,
                1.0f,
                std::fmod(t_ * 55.0f, 360.0f)
            };
            nextRipple_ = (nextRipple_ + 1) % ripples_.size();
            rippleCooldown_ = 0.45f;
        }
        prevMotion_ = state.live.motionDetected;
        if (rippleCooldown_ > 0.0f) rippleCooldown_ -= dtSec;

        for (auto& r : ripples_) {
            if (r.life > 0.0f) {
                r.radius += dtSec * 11.5f * state.animationSpeed;
                r.life = std::max(0.0f, r.life - dtSec * 0.9f);
            }
        }
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        c.clear(gfx::RGB(4, 6, 16));
        const auto& style = state.modeSettings.motionStyle;
        float mx = std::clamp(state.live.motionX, 0.0f, 1.0f) * 15.0f;
        float my = std::clamp(state.live.motionY, 0.0f, 1.0f) * 15.0f;

        if (style == "swarm") {
            // 18 glowing fireflies orbiting calm center or swarming toward motion target
            float tx = state.live.motionDetected ? mx : (7.5f + std::cos(t_ * 0.7f) * 3.0f);
            float ty = state.live.motionDetected ? my : (7.5f + std::sin(t_ * 0.9f) * 3.0f);
            for (int i = 0; i < 18; ++i) {
                float a = t_ * (1.2f + energy_ * 3.2f) + i * 2.39996f;
                float r = (1.5f + (i % 5) * 1.1f) * (1.0f - energy_ * 0.45f) + std::sin(t_ * 3.0f + i) * 0.8f;
                int px = static_cast<int>(std::round(tx + std::cos(a) * r));
                int py = static_cast<int>(std::round(ty + std::sin(a) * r));
                gfx::RGB col = gfx::lerpColor(state.primaryColor, state.accentColor,
                                              std::clamp(energy_ + (i % 4) * 0.15f, 0.0f, 1.0f));
                c.addPixel(px, py, col);
            }
        } else if (style == "aura") {
            // Presence aura breathing calm blue when idle, vivid plasma when occupied
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    float dx = x - (energy_ > 0.1f ? mx : 7.5f);
                    float dy = y - (energy_ > 0.1f ? my : 7.5f);
                    float d = std::sqrt(dx * dx + dy * dy);
                    float wave = 0.5f + 0.5f * std::sin(d * 0.85f - t_ * (2.0f + energy_ * 5.0f));
                    float falloff = std::exp(-d * (0.28f - energy_ * 0.12f));
                    float hue = std::fmod(210.0f - energy_ * 170.0f + d * 12.0f + t_ * 20.0f, 360.0f);
                    c.setPixel(x, y, gfx::hsvToRgb(hue, 0.88f, std::clamp(wave * falloff * (0.55f + energy_ * 0.65f), 0.0f, 1.0f)));
                }
            }
        } else {
            // Ripples mode: calm breathing center + expanding shockwaves on motion
            float breath = 0.35f + 0.25f * std::sin(t_ * 2.0f);
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    float d0 = std::hypot(x - 7.5f, y - 7.5f);
                    if (d0 < 3.2f) {
                        c.setPixel(x, y, state.primaryColor.scaledF(breath * (1.0f - d0 / 3.4f)));
                    }
                    for (const auto& r : ripples_) {
                        if (r.life <= 0.0f) continue;
                        float dr = std::fabs(std::hypot(x - r.cx, y - r.cy) - r.radius);
                        if (dr < 1.35f) {
                            float intensity = (1.0f - dr / 1.35f) * r.life;
                            c.addPixel(x, y, gfx::hsvToRgb(r.hue, 0.9f, intensity));
                        }
                    }
                }
            }
        }
    }

private:
    struct Ripple {
        float cx = 7.5f;
        float cy = 7.5f;
        float radius = 0.0f;
        float life = 0.0f;
        float hue = 180.0f;
    };

    float t_ = 0.0f;
    float energy_ = 0.0f;
    bool prevMotion_ = false;
    float rippleCooldown_ = 0.0f;
    size_t nextRipple_ = 0;
    std::array<Ripple, 5> ripples_{};
};

std::unique_ptr<DisplayMode> createMotionMode() {
    return std::make_unique<MotionMode>();
}

} // namespace modes
