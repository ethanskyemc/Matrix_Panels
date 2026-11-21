#include "modes/all_modes.h"

#include <cmath>
#include "core/app_state.h"

namespace modes {

class AlbumMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::AlbumArt; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        if (state.live.albumArtValid) {
            c.copyFromRGB888(state.live.albumArtRgb.data());
            // Subtle bottom playback progress / pulse bar when playing
            if (state.live.mediaPlaying) {
                float p = std::fmod(t_ * 0.15f, 1.0f);
                c.blendPixel(static_cast<int>(p * 15.0f), 15, gfx::palette::White, 140);
            }
            return;
        }

        // Fallback: Spinning retro vinyl record with glowing color label
        c.clear(gfx::RGB(8, 8, 16));
        float cx = 7.5f, cy = 7.5f;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float r = std::sqrt(dx * dx + dy * dy);
                if (r > 7.2f) continue;

                float ang = std::atan2(dy, dx) + t_ * 2.2f;
                if (r < 2.6f) {
                    // Center record label
                    float hue = std::fmod(t_ * 18.0f + 20.0f, 360.0f);
                    gfx::RGB col = (r < 0.9f) ? gfx::palette::Black : gfx::hsvToRgb(hue, 0.85f, 0.95f);
                    c.setPixel(x, y, col);
                } else {
                    // Grooved dark vinyl with rotating specular highlight
                    float sheen = std::pow(std::max(0.0f, std::cos(ang * 2.0f)), 4.0f);
                    int groove = (static_cast<int>(r * 2.0f) % 2 == 0) ? 24 : 36;
                    uint8_t v = static_cast<uint8_t>(std::min<int>(255, groove + static_cast<int>(sheen * 95.0f)));
                    c.setPixel(x, y, gfx::RGB(v, v, static_cast<uint8_t>(std::min<int>(255, v + 14))));
                }
            }
        }
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createAlbumMode() {
    return std::make_unique<AlbumMode>();
}

} // namespace modes
