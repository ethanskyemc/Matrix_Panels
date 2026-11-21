#include "modes/all_modes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "core/app_state.h"

namespace modes {

class MusicMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::MusicVisualizer; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
        float sens = std::clamp(state.modeSettings.musicSensitivity, 0.2f, 3.0f);

        for (int i = 0; i < 16; ++i) {
            float target = 0.0f;
            if (state.live.audioActive) {
                target = std::clamp(state.live.audioBands[i] * sens, 0.0f, 1.0f);
            } else {
                // Smooth musical rhythm preview when no live audio stream is present
                float beat = std::pow(0.5f + 0.5f * std::sin(t_ * 4.8f), 2.0f);
                float wave = 0.5f + 0.5f * std::sin(t_ * (2.6f + i * 0.22f) + i * 0.55f);
                float bassBoost = (i < 5) ? (0.45f + 0.55f * beat) : (0.35f + 0.45f * wave);
                target = std::clamp(wave * bassBoost * 0.92f, 0.05f, 1.0f);
            }

            // Fast attack, smooth decay
            if (target > smooth_[i]) {
                smooth_[i] += (target - smooth_[i]) * std::min(1.0f, dtSec * 22.0f);
            } else {
                smooth_[i] += (target - smooth_[i]) * std::min(1.0f, dtSec * 8.0f);
            }

            // Peak hold with gravity
            if (smooth_[i] >= peaks_[i]) {
                peaks_[i] = smooth_[i];
                peakVel_[i] = 0.0f;
            } else {
                peakVel_[i] += dtSec * 1.4f;
                peaks_[i] = std::max(0.0f, peaks_[i] - peakVel_[i] * dtSec);
            }
        }
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        c.clear(gfx::RGB(4, 4, 12));
        const auto& style = state.modeSettings.musicStyle;

        if (style == "mirror") {
            for (int x = 0; x < 16; ++x) {
                int halfH = static_cast<int>(std::round(smooth_[x] * 7.5f));
                float hue = std::fmod(x * 16.0f + t_ * 35.0f, 360.0f);
                for (int dy = 0; dy <= halfH; ++dy) {
                    float frac = static_cast<float>(dy) / 8.0f;
                    gfx::RGB col = gfx::hsvToRgb(hue + dy * 8.0f, 0.92f, 1.0f - frac * 0.2f);
                    c.setPixel(x, 7 - dy, col);
                    c.setPixel(x, 8 + dy, col);
                }
            }
        } else if (style == "radial") {
            float avgBass = (smooth_[0] + smooth_[1] + smooth_[2] + smooth_[3]) * 0.25f;
            float avgMid  = (smooth_[6] + smooth_[7] + smooth_[8] + smooth_[9]) * 0.25f;
            float rOuter = 2.0f + avgBass * 6.5f;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    float dx = x - 7.5f;
                    float dy = y - 7.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    float ang = std::atan2(dy, dx);
                    int band = static_cast<int>(((ang + 3.14159f) / (2.0f * 3.14159f)) * 16.0f) % 16;
                    float rLocal = rOuter + smooth_[band] * 2.5f;
                    if (dist <= rLocal) {
                        float hue = std::fmod(dist * 24.0f - t_ * 60.0f + avgMid * 80.0f, 360.0f);
                        float v = 1.0f - std::clamp((dist / (rLocal + 0.5f)) * 0.45f, 0.0f, 0.6f);
                        c.setPixel(x, y, gfx::hsvToRgb(hue, 0.9f, v));
                    }
                }
            }
        } else if (style == "vu") {
            float left = 0.0f, right = 0.0f;
            for (int i = 0; i < 8; ++i) {
                left += smooth_[i];
                right += smooth_[i + 8];
            }
            left /= 8.0f;
            right /= 8.0f;
            auto drawBar = [&](int xStart, float level) {
                int h = static_cast<int>(std::round(level * 15.0f));
                for (int row = 0; row < h; ++row) {
                    int y = 15 - row;
                    gfx::RGB col = (row < 9) ? gfx::palette::Green
                                 : (row < 13) ? gfx::palette::Amber
                                              : gfx::palette::Red;
                    c.fillRect(xStart, y, 5, 1, col);
                }
            };
            drawBar(2, left);
            drawBar(9, right);
        } else {
            // Classic 16-band spectrum with acrylic glow gradient and crisp white peak dots
            for (int x = 0; x < 16; ++x) {
                int barH = static_cast<int>(std::round(smooth_[x] * 15.0f));
                for (int row = 0; row < barH; ++row) {
                    int y = 15 - row;
                    float frac = static_cast<float>(row) / 15.0f;
                    gfx::RGB col = gfx::lerpColor(state.primaryColor, state.accentColor, frac);
                    if (row >= 13) col = gfx::lerpColor(col, gfx::palette::WarmWhite, 0.5f);
                    c.setPixel(x, y, col);
                }
                int peakY = 15 - static_cast<int>(std::round(peaks_[x] * 15.0f));
                if (peakY >= 0 && peakY < 16 && peaks_[x] > 0.06f) {
                    c.setPixel(x, peakY, gfx::palette::White);
                }
            }
        }
    }

private:
    float t_ = 0.0f;
    std::array<float, 16> smooth_{};
    std::array<float, 16> peaks_{};
    std::array<float, 16> peakVel_{};
};

std::unique_ptr<DisplayMode> createMusicMode() {
    return std::make_unique<MusicMode>();
}

} // namespace modes
