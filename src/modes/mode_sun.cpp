#include "modes/all_modes.h"

#include <cmath>
#include <string>
#include "core/app_state.h"
#include "core/astro.h"
#include "gfx/font.h"

namespace modes {

class SunMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::SunriseSunset; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        const auto& tm = state.live.time;
        int nowMin = tm.hour * 60 + tm.minute;
        core::SunTimes st = core::computeSunTimes(
            tm.year, tm.month, tm.day,
            state.location.latitude, state.location.longitude,
            state.location.tzOffsetMinutes,
            nowMin,
            state.modeSettings.sunGradualWindowMin
        );

        // Alternate between Sunrise and Sunset info every 5 seconds
        bool showSunrise = (static_cast<int>(t_ / 5.0f) % 2) == 0;

        // Sky gradient reflects gradual dawn/dusk lighting effect when enabled
        gfx::RGB skyTop, skyBot;
        if (state.modeSettings.sunGradualEffect && (st.dawnGlow > 0.05f || st.duskGlow > 0.05f)) {
            float g = std::max(st.dawnGlow, st.duskGlow);
            skyTop = gfx::lerpColor(gfx::RGB(12, 18, 48), gfx::RGB(145, 52, 118), g);
            skyBot = gfx::lerpColor(gfx::RGB(30, 20, 42), gfx::RGB(255, 138, 38), g);
        } else if (showSunrise) {
            skyTop = gfx::RGB(26, 40, 96);
            skyBot = gfx::RGB(170, 74, 42);
        } else {
            skyTop = gfx::RGB(32, 16, 72);
            skyBot = gfx::RGB(165, 48, 68);
        }
        c.drawGradientV(skyTop, skyBot);

        // Horizon arc (rows 0..8)
        for (int x = 1; x <= 14; ++x) {
            float u = static_cast<float>(x - 1) / 13.0f;
            int ay = 8 - static_cast<int>(std::round(std::sin(u * 3.14159f) * 5.2f));
            c.blendPixel(x, ay, gfx::palette::Amber, 75);
        }
        // Horizon line
        c.drawHLine(0, 8, 16, gfx::RGB(90, 56, 48));

        // Sun orb animated gently along horizon or current day progress
        float sunU = st.isDaytime ? st.dayProgress : (showSunrise ? 0.14f : 0.86f);
        int sunX = 1 + static_cast<int>(std::round(sunU * 13.0f));
        int sunY = 8 - static_cast<int>(std::round(std::sin(sunU * 3.14159f) * 5.2f));
        c.fillCircle(sunX, sunY, 1, gfx::palette::Yellow);
        c.setPixel(sunX, sunY, gfx::palette::WarmWhite);

        // Small up/down arrow icon on left indicating Rise (^) vs Set (v)
        if (showSunrise) {
            c.setPixel(1, 1, gfx::palette::Amber);
            c.setPixel(0, 2, gfx::palette::Amber);
            c.setPixel(2, 2, gfx::palette::Amber);
            c.setPixel(1, 2, gfx::palette::Yellow);
        } else {
            c.setPixel(0, 1, gfx::palette::Coral);
            c.setPixel(2, 1, gfx::palette::Coral);
            c.setPixel(1, 1, gfx::palette::Orange);
            c.setPixel(1, 2, gfx::palette::Coral);
        }

        // Bottom readable time readout (y=10..14)
        std::string timeStr = core::formatMinutesHHMM(
            showSunrise ? st.sunriseMinutes : st.sunsetMinutes,
            state.modeSettings.clock24h
        );
        gfx::drawTextCentered3x5(c, 10, timeStr,
                                 showSunrise ? gfx::palette::WarmWhite : gfx::RGB(255, 210, 180),
                                 1);
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createSunMode() {
    return std::make_unique<SunMode>();
}

} // namespace modes
