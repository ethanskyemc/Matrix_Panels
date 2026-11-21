#include "modes/all_modes.h"

#include <cmath>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class ClockMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Clock; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        const auto& tm = state.live.time;
        const auto& ms = state.modeSettings;

        // Subtle dark ambient backdrop for acrylic contrast
        gfx::RGB bgTop = gfx::RGB(4, 6, 16);
        gfx::RGB bgBot = gfx::RGB(10, 8, 22);
        if (ms.clockColorStyle == "time_of_day") {
            if (tm.hour >= 6 && tm.hour < 11) {
                bgTop = gfx::RGB(18, 12, 6);
                bgBot = gfx::RGB(10, 6, 14);
            } else if (tm.hour >= 11 && tm.hour < 17) {
                bgTop = gfx::RGB(6, 14, 24);
                bgBot = gfx::RGB(4, 8, 16);
            } else if (tm.hour >= 17 && tm.hour < 21) {
                bgTop = gfx::RGB(22, 8, 18);
                bgBot = gfx::RGB(10, 6, 20);
            }
        }
        c.drawGradientV(bgTop, bgBot);

        int dispHour = tm.hour;
        bool isPm = tm.hour >= 12;
        if (!ms.clock24h) {
            dispHour = tm.hour % 12;
            if (dispHour == 0) dispHour = 12;
        }

        gfx::RGB hourColor = state.primaryColor;
        gfx::RGB minColor = state.accentColor;

        if (ms.clockColorStyle == "solid") {
            minColor = hourColor;
        } else if (ms.clockColorStyle == "rainbow") {
            hourColor = gfx::hsvToRgb(std::fmod(t_ * 28.0f, 360.0f), 0.85f, 1.0f);
            minColor  = gfx::hsvToRgb(std::fmod(t_ * 28.0f + 90.0f, 360.0f), 0.85f, 1.0f);
        } else if (ms.clockColorStyle == "time_of_day") {
            if (tm.hour >= 6 && tm.hour < 12) {
                hourColor = gfx::palette::Amber;
                minColor = gfx::palette::Coral;
            } else if (tm.hour >= 12 && tm.hour < 18) {
                hourColor = gfx::palette::Cyan;
                minColor = gfx::palette::WarmWhite;
            } else if (tm.hour >= 18 && tm.hour < 22) {
                hourColor = gfx::palette::Coral;
                minColor = gfx::palette::Purple;
            } else {
                hourColor = gfx::palette::Indigo;
                minColor = gfx::palette::SkyBlue;
            }
        }

        // Stacked bold 4x6 digits: HH on y=1..6, MM on y=8..13
        gfx::drawTwoDigits4x6(c, 3, 1, dispHour, hourColor, ms.clock24h);
        gfx::drawTwoDigits4x6(c, 3, 8, tm.minute, minColor, true);

        // Pulsing beat dots on right margin (x=14)
        float pulse = 0.5f + 0.5f * std::sin(t_ * 3.14159f * 2.0f);
        gfx::RGB dotCol = gfx::lerpColor(hourColor, minColor, 0.5f).scaledF(0.25f + 0.75f * pulse);
        c.setPixel(14, 5, dotCol);
        c.setPixel(14, 8, dotCol);

        // AM/PM indicator pixel at top-left when 12h
        if (!ms.clock24h) {
            c.setPixel(1, 1, isPm ? gfx::palette::Amber : gfx::palette::SkyBlue);
        }

        // Smooth seconds progress bar along bottom row (y=15)
        if (ms.clockShowSecondsBar) {
            float secFrac = (static_cast<float>(tm.second) + std::fmod(t_, 1.0f)) / 60.0f;
            if (secFrac > 1.0f) secFrac = 1.0f;
            c.drawProgressBar(1, 15, 14, secFrac,
                              gfx::lerpColor(hourColor, minColor, secFrac),
                              gfx::RGB(18, 22, 34));
        }
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createClockMode() {
    return std::make_unique<ClockMode>();
}

} // namespace modes
