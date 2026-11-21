#include "modes/all_modes.h"

#include <cmath>
#include <cstdio>
#include <string>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class WeatherMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Weather; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        const auto cond = state.live.weatherCondition;

        // Sky backdrop tailored to condition
        switch (cond) {
            case core::WeatherCondition::ClearDay:
                c.drawGradientV(gfx::RGB(14, 42, 92), gfx::RGB(8, 22, 54));
                break;
            case core::WeatherCondition::ClearNight:
                c.drawGradientV(gfx::RGB(4, 8, 28), gfx::RGB(10, 12, 38));
                break;
            case core::WeatherCondition::Storm:
                c.drawGradientV(gfx::RGB(14, 14, 28), gfx::RGB(6, 6, 14));
                break;
            case core::WeatherCondition::Snow:
                c.drawGradientV(gfx::RGB(16, 26, 46), gfx::RGB(10, 16, 30));
                break;
            default:
                c.drawGradientV(gfx::RGB(12, 24, 48), gfx::RGB(8, 14, 28));
                break;
        }

        drawWeatherIcon(c, cond);

        if (state.modeSettings.weatherShowTemp) {
            float temp = state.live.weatherTempC;
            if (state.location.useFahrenheit) {
                temp = temp * 9.0f / 5.0f + 32.0f;
            }
            int ti = static_cast<int>(std::round(temp));
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", ti);
            std::string s(buf);

            // Color temperature text subtly by warmth
            gfx::RGB tCol = gfx::palette::White;
            if (state.live.weatherTempC <= 2.0f)       tCol = gfx::palette::Cyan;
            else if (state.live.weatherTempC <= 14.0f) tCol = gfx::palette::SkyBlue;
            else if (state.live.weatherTempC >= 28.0f) tCol = gfx::palette::Coral;
            else if (state.live.weatherTempC >= 22.0f) tCol = gfx::palette::Amber;

            int tw = gfx::measureText3x5(s, 1);
            int startX = std::max(0, (16 - (tw + 3)) / 2);
            gfx::drawText3x5(c, startX, 10, s, tCol, 1);
            // Tiny degree dot
            c.setPixel(startX + tw + 1, 10, tCol);
            c.setPixel(startX + tw + 2, 10, tCol);
            c.setPixel(startX + tw + 1, 11, tCol);
            c.setPixel(startX + tw + 2, 11, tCol);
        }
    }

private:
    float t_ = 0.0f;

    void drawCloud(gfx::Canvas& c, int ox, int oy, gfx::RGB bright, gfx::RGB shade) {
        c.fillRect(ox + 1, oy + 2, 7, 2, shade);
        c.fillRect(ox + 2, oy + 1, 5, 2, bright);
        c.setPixel(ox + 3, oy, bright);
        c.setPixel(ox + 4, oy, bright);
        c.setPixel(ox, oy + 3, shade);
        c.setPixel(ox + 8, oy + 3, shade);
    }

    void drawSun(gfx::Canvas& c, int cx, int cy) {
        c.fillRect(cx - 1, cy - 1, 3, 3, gfx::palette::Yellow);
        c.setPixel(cx, cy, gfx::palette::WarmWhite);
        // Rotating animated rays
        float phase = t_ * 2.0f;
        for (int i = 0; i < 8; ++i) {
            float a = phase + i * (3.14159f / 4.0f);
            float r = 2.6f + 0.6f * std::sin(t_ * 4.0f + i);
            int rx = static_cast<int>(std::round(cx + std::cos(a) * r));
            int ry = static_cast<int>(std::round(cy + std::sin(a) * r));
            c.blendPixel(rx, ry, gfx::palette::Amber, 210);
        }
    }

    void drawWeatherIcon(gfx::Canvas& c, core::WeatherCondition cond) {
        int drift = static_cast<int>(std::round(std::sin(t_ * 0.9f) * 1.2f));
        switch (cond) {
            case core::WeatherCondition::ClearDay:
                drawSun(c, 8, 4);
                break;

            case core::WeatherCondition::ClearNight: {
                // Crescent moon + twinkling stars
                c.fillCircle(6, 4, 2, gfx::palette::WarmWhite);
                c.fillCircle(7, 3, 2, gfx::RGB(6, 10, 32));
                int tw = static_cast<int>(150 + 100 * std::sin(t_ * 3.0f));
                c.setPixel(12, 2, gfx::palette::Yellow.scaled(static_cast<uint8_t>(tw)));
                c.setPixel(2, 6, gfx::palette::SkyBlue.scaled(static_cast<uint8_t>(255 - tw / 2)));
                c.setPixel(13, 6, gfx::palette::White.scaled(static_cast<uint8_t>(tw)));
                break;
            }

            case core::WeatherCondition::PartlyCloudy:
                drawSun(c, 5, 3);
                drawCloud(c, 5 + drift, 3, gfx::RGB(230, 238, 250), gfx::RGB(160, 178, 204));
                break;

            case core::WeatherCondition::Cloudy:
                drawCloud(c, 3 + drift, 2, gfx::RGB(190, 200, 215), gfx::RGB(120, 132, 150));
                break;

            case core::WeatherCondition::Rain: {
                drawCloud(c, 3, 1, gfx::RGB(150, 165, 190), gfx::RGB(95, 110, 135));
                // Animated falling raindrops
                for (int i = 0; i < 4; ++i) {
                    int rx = 4 + i * 2;
                    int ry = 5 + static_cast<int>(std::fmod(t_ * 7.5f + i * 1.7f, 4.0f));
                    c.setPixel(rx, ry, gfx::palette::Cyan);
                }
                break;
            }

            case core::WeatherCondition::Storm: {
                drawCloud(c, 3, 1, gfx::RGB(110, 118, 138), gfx::RGB(68, 74, 92));
                // Periodic lightning flash
                float flashCycle = std::fmod(t_, 3.2f);
                if (flashCycle < 0.22f || (flashCycle > 0.30f && flashCycle < 0.42f)) {
                    c.setPixel(8, 5, gfx::palette::Yellow);
                    c.setPixel(7, 6, gfx::palette::White);
                    c.setPixel(8, 7, gfx::palette::Yellow);
                    c.setPixel(7, 8, gfx::palette::Amber);
                } else {
                    for (int i = 0; i < 3; ++i) {
                        int rx = 5 + i * 3;
                        int ry = 5 + static_cast<int>(std::fmod(t_ * 9.0f + i * 2.1f, 4.0f));
                        c.setPixel(rx, ry, gfx::palette::SkyBlue);
                    }
                }
                break;
            }

            case core::WeatherCondition::Snow: {
                drawCloud(c, 3, 1, gfx::RGB(200, 218, 240), gfx::RGB(130, 150, 180));
                for (int i = 0; i < 5; ++i) {
                    float fy = std::fmod(t_ * 2.8f + i * 1.3f, 4.5f);
                    int sx = 3 + i * 2 + static_cast<int>(std::round(std::sin(t_ * 2.0f + i)));
                    int sy = 5 + static_cast<int>(fy);
                    c.setPixel(sx, sy, gfx::palette::White);
                }
                break;
            }

            case core::WeatherCondition::Fog:
            case core::WeatherCondition::Windy: {
                for (int row = 0; row < 3; ++row) {
                    int x0 = 2 + static_cast<int>(std::round(std::sin(t_ * 2.0f + row * 1.4f) * 2.0f));
                    gfx::RGB col = (cond == core::WeatherCondition::Windy)
                                       ? gfx::palette::Teal
                                       : gfx::RGB(170, 184, 198);
                    c.drawHLine(x0, 2 + row * 2, 10, col);
                }
                break;
            }
        }
    }
};

std::unique_ptr<DisplayMode> createWeatherMode() {
    return std::make_unique<WeatherMode>();
}

} // namespace modes
