#include "modes/all_modes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class AirQualityMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::AirQuality; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        int aqi = std::clamp(state.live.aqi, 0, 500);
        gfx::RGB aqiCol = aqiColor(aqi);

        // Dark tinted background reflecting air quality health band
        c.drawGradientV(aqiCol.scaled(32), gfx::RGB(6, 10, 18));

        // Cycle pages: 0 = Temp, 1 = AQI, 2 = Humidity, (3 = CO2 if enabled)
        int numPages = state.modeSettings.airQualityShowCo2 ? 4 : 3;
        int page = static_cast<int>(t_ / 4.0f) % numPages;

        // Top AQI status bar & leaf/air icon (y=1..7)
        float breath = 0.78f + 0.22f * std::sin(t_ * 2.4f);
        gfx::RGB glow = aqiCol.scaledF(breath);

        if (page == 0) {
            // Thermometer icon
            c.fillRect(7, 1, 2, 5, gfx::palette::WarmWhite);
            c.fillCircle(7, 6, 1, gfx::palette::Coral);
            c.setPixel(8, 6, gfx::palette::Coral);
            c.setPixel(7, 3, gfx::palette::Coral);

            float temp = state.live.indoorTempC;
            if (state.location.useFahrenheit) temp = temp * 9.0f / 5.0f + 32.0f;
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d*", static_cast<int>(std::round(temp)));
            gfx::drawTextCentered3x5(c, 10, std::string(buf), gfx::palette::WarmWhite, 1);
        } else if (page == 1) {
            // Air quality leaf icon
            c.fillCircle(7, 4, 2, glow);
            c.setPixel(9, 2, glow);
            c.setPixel(10, 2, glow);
            c.drawLine(5, 6, 8, 3, gfx::palette::White);

            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", aqi);
            gfx::drawTextCentered3x5(c, 10, std::string(buf), aqiCol, 1);
        } else if (page == 2) {
            // Water droplet icon for humidity
            c.setPixel(7, 1, gfx::palette::SkyBlue);
            c.fillRect(6, 2, 3, 2, gfx::palette::SkyBlue);
            c.fillRect(5, 4, 5, 2, gfx::palette::Cyan);
            c.fillRect(6, 6, 3, 1, gfx::palette::Cyan);

            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(std::round(state.live.indoorHumidity)));
            gfx::drawTextCentered3x5(c, 10, std::string(buf), gfx::palette::Cyan, 1);
        } else {
            // CO2 ppm icon
            gfx::drawTextCentered3x5(c, 2, "CO2", glow, 1);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", std::clamp(state.live.co2Ppm, 0, 9999));
            gfx::drawTextCentered3x5(c, 10, std::string(buf), gfx::palette::WarmWhite, 1);
        }

        // Bottom AQI health gauge bar (y=15)
        float norm = std::clamp(static_cast<float>(aqi) / 200.0f, 0.05f, 1.0f);
        c.drawProgressBar(1, 15, 14, norm, aqiCol, gfx::RGB(18, 24, 34));
    }

private:
    float t_ = 0.0f;

    static gfx::RGB aqiColor(int aqi) {
        if (aqi <= 50)  return gfx::palette::Green;
        if (aqi <= 100) return gfx::palette::Yellow;
        if (aqi <= 150) return gfx::palette::Orange;
        if (aqi <= 200) return gfx::palette::Red;
        return gfx::palette::Purple;
    }
};

std::unique_ptr<DisplayMode> createAirQualityMode() {
    return std::make_unique<AirQualityMode>();
}

} // namespace modes
