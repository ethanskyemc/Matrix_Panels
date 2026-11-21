#include "modes/all_modes.h"

#include <cmath>
#include "core/app_state.h"

namespace modes {

// ---- 1. SOLAR SYSTEM ----
class SolarSystemMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::SolarSystem; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState&, const core::MediaStore&) override {
        c.clear(gfx::RGB(3, 4, 12));

        // Subtle distant stars
        static constexpr int kStars[6][2] = {{0,0},{15,1},{1,14},{14,15},{0,9},{15,6}};
        for (int i = 0; i < 6; ++i) {
            float tw = 0.35f + 0.45f * (0.5f + 0.5f * std::sin(t_ * 2.0f + i * 2.1f));
            c.setPixel(kStars[i][0], kStars[i][1], gfx::RGB(150, 175, 230).scaledF(tw));
        }

        const float cx = 7.5f, cy = 7.5f;

        // Subtle orbital rings
        const float orbitRadii[4] = {2.6f, 4.1f, 5.6f, 7.0f};
        for (float r : orbitRadii) {
            for (int i = 0; i < 32; ++i) {
                float a = i * (2.0f * 3.14159f / 32.0f);
                int ox = static_cast<int>(std::round(cx + std::cos(a) * r));
                int oy = static_cast<int>(std::round(cy + std::sin(a) * r));
                c.blendPixel(ox, oy, gfx::RGB(34, 46, 86), 75);
            }
        }

        // Pulsing Sun at center (2x2 core + corona)
        float sunPulse = 0.85f + 0.15f * std::sin(t_ * 4.5f);
        c.fillRect(7, 7, 2, 2, gfx::palette::Yellow.scaledF(sunPulse));
        c.blendPixel(6, 7, gfx::palette::Orange, 120);
        c.blendPixel(9, 8, gfx::palette::Orange, 120);
        c.blendPixel(7, 6, gfx::palette::Orange, 120);
        c.blendPixel(8, 9, gfx::palette::Orange, 120);

        struct Planet { float r; float speed; float phase; gfx::RGB color; bool hasRing; };
        const Planet planets[5] = {
            {2.5f, 2.6f, 0.4f, gfx::RGB(210, 185, 145), false}, // Mercury/Venus
            {4.0f, 1.6f, 2.1f, gfx::palette::SkyBlue,   false}, // Earth
            {5.3f, 1.1f, 4.5f, gfx::palette::Coral,     false}, // Mars
            {6.4f, 0.72f, 1.0f, gfx::palette::Amber,    false}, // Jupiter
            {7.2f, 0.48f, 3.6f, gfx::palette::Gold,     true},  // Saturn
        };

        for (const auto& p : planets) {
            float a = t_ * p.speed + p.phase;
            int px = static_cast<int>(std::round(cx + std::cos(a) * p.r));
            int py = static_cast<int>(std::round(cy + std::sin(a) * p.r));
            if (p.hasRing) {
                c.blendPixel(px - 1, py, gfx::RGB(220, 195, 130), 170);
                c.blendPixel(px + 1, py, gfx::RGB(220, 195, 130), 170);
            }
            c.setPixel(px, py, p.color);
        }
    }

private:
    float t_ = 0.0f;
};

// ---- 2. SPINNING GLOBE (3D Orthographic Earth) ----
class SpinningGlobeMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::SpinningGlobe; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState&, const core::MediaStore&) override {
        c.clear(gfx::RGB(3, 5, 14));

        // Corner stars
        c.setPixel(1, 1, gfx::RGB(120, 145, 200));
        c.setPixel(14, 2, gfx::RGB(150, 165, 210));
        c.setPixel(1, 13, gfx::RGB(100, 125, 180));
        c.setPixel(14, 14, gfx::RGB(140, 160, 215));

        // 32x16 simplified equirectangular Earth land mask (1 = land, 0 = ocean)
        // Americas (left), Europe/Africa (center), Asia/Australia (right)
        static constexpr uint32_t kEarthMap[16] = {
            0x00000000u, // North pole ice (handled separately)
            0x0F003E1Fu,
            0x1F807F3Fu,
            0x3FC0FF7Eu,
            0x1F807FFCu,
            0x0F003FF8u,
            0x06001FF0u,
            0x03803FE0u,
            0x07C03EC0u,
            0x03E01E00u,
            0x01E01E1Cu,
            0x00E00C3Eu,
            0x0060081Cu,
            0x00400000u,
            0x00000000u,
            0x00000000u
        };

        const float cx = 7.5f, cy = 7.5f, radius = 6.3f;
        const float spinLon = t_ * 0.85f; // radians

        // Light vector from upper-left front
        const float lx = -0.45f, ly = -0.35f, lz = 0.82f;

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float nx = (x - cx) / radius;
                float ny = (y - cy) / radius;
                float r2 = nx * nx + ny * ny;

                if (r2 > 1.0f) {
                    // Subtle atmospheric blue halo around globe rim
                    float rim = std::sqrt(r2) - 1.0f;
                    if (rim < 0.16f) {
                        c.blendPixel(x, y, gfx::RGB(45, 135, 255), static_cast<uint8_t>((1.0f - rim / 0.16f) * 95.0f));
                    }
                    continue;
                }

                float nz = std::sqrt(std::max(0.0f, 1.0f - r2));
                float lat = std::asin(std::clamp(ny, -1.0f, 1.0f)); // -pi/2 .. pi/2
                float lon = std::atan2(nx, nz) + spinLon;

                // Map (lon, lat) -> equirectangular map coordinates
                float u = std::fmod((lon / (2.0f * 3.14159265f)) + 10.0f, 1.0f);
                float v = (lat / 3.14159265f) + 0.5f;

                int mapX = static_cast<int>(u * 32.0f) & 31;
                int mapY = std::clamp(static_cast<int>(v * 16.0f), 0, 15);

                bool isPolarIce = (std::fabs(lat) > 1.18f);
                bool isLand = (kEarthMap[mapY] & (1u << (31 - mapX))) != 0;

                float shade = std::clamp(nx * lx + ny * ly + nz * lz, 0.18f, 1.0f);
                gfx::RGB baseCol;
                if (isPolarIce) {
                    baseCol = gfx::RGB(225, 240, 255);
                } else if (isLand) {
                    baseCol = (std::fabs(lat) < 0.35f) ? gfx::RGB(56, 195, 76) : gfx::RGB(42, 162, 86);
                } else {
                    baseCol = gfx::RGB(18, 86, 215);
                }

                // Cloud wisp highlight
                float cloud = std::sin(lon * 3.0f + lat * 4.0f + t_ * 0.35f);
                if (cloud > 0.86f) {
                    baseCol = gfx::lerpColor(baseCol, gfx::palette::White, 0.55f);
                }

                gfx::RGB lit = baseCol.scaledF(shade);
                // Soft limb blue atmosphere blend
                if (nz < 0.35f) {
                    lit = gfx::lerpColor(lit, gfx::RGB(70, 168, 255), (0.35f - nz) * 1.4f);
                }
                c.setPixel(x, y, lit);
            }
        }
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createSolarSystemMode() {
    return std::make_unique<SolarSystemMode>();
}
std::unique_ptr<DisplayMode> createSpinningGlobeMode() {
    return std::make_unique<SpinningGlobeMode>();
}

} // namespace modes
