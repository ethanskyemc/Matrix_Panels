#include "modes/all_modes.h"

#include <cmath>
#include <string>
#include "core/app_state.h"
#include "core/media_store.h"

namespace modes {

class PixelArtMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::PixelArt; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore& media) override {
        float speed = std::max(0.1f, state.animationSpeed);
        t_ += dtSec * speed;

        const core::MediaAsset* asset = media.get(state.modeSettings.pixelArtScene);
        if (asset && !asset->frames.empty()) {
            if (lastMediaId_ != asset->id) {
                lastMediaId_ = asset->id;
                frameIndex_ = 0;
                frameAccumMs_ = 0.0f;
            }
            if (asset->frames.size() > 1) {
                frameAccumMs_ += dtSec * 1000.0f * speed;
                uint16_t delay = asset->frames[frameIndex_ % asset->frames.size()].delayMs;
                while (frameAccumMs_ >= delay) {
                    frameAccumMs_ -= delay;
                    frameIndex_ = (frameIndex_ + 1) % asset->frames.size();
                    delay = asset->frames[frameIndex_].delayMs;
                }
            }
        }
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore& media) override {
        const std::string& scene = state.modeSettings.pixelArtScene;

        // 1. Check user-uploaded image / animated GIF (.mpx) first!
        if (const core::MediaAsset* asset = media.get(scene)) {
            if (!asset->frames.empty()) {
                const auto& f = asset->frames[frameIndex_ % asset->frames.size()];
                c.copyFromRGB888(f.rgb888.data());
                return;
            }
        }

        // 2. Built-in handcrafted animated 16x16 pixel art scenes
        if (scene == "invader")       renderInvader(c);
        else if (scene == "coffee")   renderCoffee(c);
        else if (scene == "cat")      renderCat(c);
        else if (scene == "synth")    renderSynthwave(c);
        else if (scene == "bonsai")   renderBonsai(c);
        else                          renderCabin(c);
    }

private:
    float t_ = 0.0f;
    std::string lastMediaId_;
    size_t frameIndex_ = 0;
    float frameAccumMs_ = 0.0f;

    void renderCabin(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(10, 16, 44), gfx::RGB(24, 34, 68));
        // Stars
        c.setPixel(2, 1, gfx::palette::WarmWhite);
        c.setPixel(13, 2, gfx::palette::Yellow);
        // Chimney + rising smoke puffs
        c.fillRect(10, 4, 2, 3, gfx::RGB(130, 62, 54));
        for (int i = 0; i < 3; ++i) {
            float p = std::fmod(t_ * 0.9f + i * 0.33f, 1.0f);
            int sx = 10 + static_cast<int>(std::round(std::sin(p * 4.0f + i) * 1.2f));
            int sy = 3 - static_cast<int>(p * 3.5f);
            c.blendPixel(sx, sy, gfx::RGB(190, 200, 220), static_cast<uint8_t>((1.0f - p) * 180.0f));
        }
        // Roof
        for (int r = 0; r < 4; ++r) {
            c.drawHLine(4 - r, 8 - r, 8 + r * 2, gfx::RGB(172, 48, 42));
        }
        // Cabin walls
        c.fillRect(3, 9, 10, 5, gfx::RGB(136, 82, 44));
        // Warm glowing window
        float flick = 0.85f + 0.15f * std::sin(t_ * 6.0f);
        c.fillRect(5, 10, 3, 3, gfx::palette::Amber.scaledF(flick));
        // Door
        c.fillRect(9, 10, 2, 4, gfx::RGB(82, 44, 24));
        // Ground
        c.fillRect(0, 14, 16, 2, gfx::RGB(34, 106, 56));
    }

    void renderInvader(gfx::Canvas& c) {
        c.clear(gfx::RGB(6, 4, 18));
        bool frameB = (static_cast<int>(t_ * 2.5f) % 2) == 1;
        int ox = 2 + static_cast<int>(std::round(std::sin(t_ * 1.5f) * 1.5f));
        gfx::RGB col = gfx::hsvToRgb(std::fmod(t_ * 45.0f, 360.0f), 0.9f, 1.0f);
        static constexpr uint16_t kRowsA[8] = {
            0b00100000100,
            0b00010001000,
            0b00111111100,
            0b01101110110,
            0b11111111111,
            0b10111111101,
            0b10100000101,
            0b00011011000
        };
        static constexpr uint16_t kRowsB[8] = {
            0b00100000100,
            0b10010001001,
            0b10111111101,
            0b11101110111,
            0b11111111111,
            0b01111111110,
            0b00100000100,
            0b01000000010
        };
        const uint16_t* rows = frameB ? kRowsB : kRowsA;
        for (int r = 0; r < 8; ++r) {
            for (int bit = 0; bit < 11; ++bit) {
                if (rows[r] & (1 << (10 - bit))) {
                    c.setPixel(ox + bit, 4 + r, col);
                }
            }
        }
    }

    void renderCoffee(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(22, 14, 18), gfx::RGB(38, 24, 18));
        // Rising steam wisps
        for (int s = 0; s < 3; ++s) {
            for (int y = 1; y <= 5; ++y) {
                int x = 5 + s * 2 + static_cast<int>(std::round(std::sin(t_ * 3.0f - y * 0.8f + s) * 0.9f));
                uint8_t a = static_cast<uint8_t>(40 + y * 28);
                c.blendPixel(x, y, gfx::palette::WarmWhite, a);
            }
        }
        // Mug
        c.fillRect(4, 7, 7, 7, gfx::RGB(232, 224, 212));
        c.fillRect(5, 7, 5, 1, gfx::RGB(108, 62, 30)); // Coffee surface
        // Handle
        c.drawVLine(12, 8, 4, gfx::RGB(210, 200, 188));
        c.setPixel(11, 8, gfx::RGB(210, 200, 188));
        c.setPixel(11, 11, gfx::RGB(210, 200, 188));
        // Saucer
        c.drawHLine(3, 14, 10, gfx::RGB(185, 175, 165));
    }

    void renderCat(gfx::Canvas& c) {
        c.clear(gfx::RGB(16, 12, 32));
        // Ears
        c.setPixel(4, 3, gfx::palette::Orange);
        c.setPixel(5, 4, gfx::palette::Orange);
        c.setPixel(4, 4, gfx::palette::Orange);
        c.setPixel(11, 3, gfx::palette::Orange);
        c.setPixel(10, 4, gfx::palette::Orange);
        c.setPixel(11, 4, gfx::palette::Orange);
        // Face
        c.fillRect(4, 5, 8, 7, gfx::palette::Orange);
        c.fillRect(5, 12, 6, 1, gfx::palette::Orange);
        // Blinking eyes
        bool blink = std::fmod(t_, 3.5f) > 3.28f;
        if (blink) {
            c.drawHLine(5, 8, 2, gfx::palette::Black);
            c.drawHLine(9, 8, 2, gfx::palette::Black);
        } else {
            c.fillRect(5, 7, 2, 2, gfx::palette::Lime);
            c.fillRect(9, 7, 2, 2, gfx::palette::Lime);
            c.setPixel(6, 8, gfx::palette::Black);
            c.setPixel(10, 8, gfx::palette::Black);
        }
        // Pink nose & whiskers
        c.fillRect(7, 9, 2, 1, gfx::palette::Pink);
        c.drawHLine(2, 9, 2, gfx::palette::WarmWhite);
        c.drawHLine(12, 9, 2, gfx::palette::WarmWhite);
    }

    void renderSynthwave(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(22, 8, 56), gfx::RGB(130, 24, 96));
        // Striped retro sun
        for (int y = 1; y <= 9; ++y) {
            for (int x = 3; x <= 12; ++x) {
                float dx = x - 7.5f, dy = y - 5.5f;
                if (dx * dx + dy * dy <= 20.0f) {
                    if (y >= 6 && (y % 2 == 1)) continue; // horizontal retro cutouts
                    float u = (y - 1.0f) / 8.0f;
                    c.setPixel(x, y, gfx::lerpColor(gfx::palette::Yellow, gfx::palette::Magenta, u));
                }
            }
        }
        // Perspective grid scrolling towards viewer
        c.fillRect(0, 10, 16, 6, gfx::RGB(14, 4, 34));
        for (int row = 0; row < 6; ++row) {
            float p = std::fmod(row * 0.25f + t_ * 0.8f, 1.0f);
            int gy = 10 + static_cast<int>(p * p * 5.9f);
            c.drawHLine(0, gy, 16, gfx::RGB(210, 42, 190));
        }
        for (int i = -2; i <= 2; ++i) {
            c.drawLine(8 + i, 10, 8 + i * 4, 15, gfx::palette::Cyan);
        }
    }

    void renderBonsai(gfx::Canvas& c) {
        c.drawGradientV(gfx::RGB(14, 22, 34), gfx::RGB(24, 32, 46));
        // Ceramic pot
        c.fillRect(4, 12, 8, 3, gfx::RGB(44, 102, 148));
        c.drawHLine(3, 12, 10, gfx::RGB(66, 134, 186));
        // Twisted trunk
        c.drawLine(8, 11, 7, 8, gfx::RGB(115, 72, 42));
        c.drawLine(7, 8, 9, 5, gfx::RGB(115, 72, 42));
        c.drawLine(7, 8, 4, 6, gfx::RGB(115, 72, 42));
        // Gently swaying foliage pads
        int sway = static_cast<int>(std::round(std::sin(t_ * 1.4f) * 0.6f));
        c.fillCircle(9 + sway, 4, 2, gfx::RGB(42, 188, 86));
        c.fillCircle(4 + sway, 6, 2, gfx::RGB(34, 165, 74));
        c.fillCircle(11 + sway, 7, 1, gfx::RGB(56, 210, 98));
    }
};

std::unique_ptr<DisplayMode> createPixelArtMode() {
    return std::make_unique<PixelArtMode>();
}

} // namespace modes
