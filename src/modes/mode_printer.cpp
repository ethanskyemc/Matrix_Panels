#include "modes/all_modes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "core/app_state.h"
#include "gfx/font.h"

namespace modes {

class PrinterMode : public DisplayMode {
public:
    ModeId id() const override { return ModeId::Printer3D; }

    void update(float dtSec, const core::AppState& state, const core::MediaStore&) override {
        t_ += dtSec * state.animationSpeed;
    }

    void render(gfx::Canvas& c, const core::AppState& state, const core::MediaStore&) override {
        const auto pState = state.live.printerState;
        float pct = std::clamp(state.live.printerProgress, 0.0f, 100.0f);

        // Completion alert animation!
        if (pState == core::PrinterState::Complete || pct >= 99.9f) {
            float pulse = 0.5f + 0.5f * std::sin(t_ * 5.0f);
            c.clear(gfx::RGB(4, 24, 12));
            if (state.modeSettings.printerAlertFlash) {
                gfx::RGB border = gfx::palette::Green.scaledF(0.45f + 0.55f * pulse);
                c.drawRect(0, 0, 16, 16, border);
            }
            // Crisp green checkmark icon
            c.drawLine(3, 8, 6, 11, gfx::palette::Lime);
            c.drawLine(6, 11, 12, 4, gfx::palette::Lime);
            c.drawLine(3, 7, 6, 10, gfx::palette::Green);
            c.drawLine(6, 10, 12, 3, gfx::palette::Green);
            return;
        }

        // Error alert state
        if (pState == core::PrinterState::Error) {
            float flash = 0.5f + 0.5f * std::sin(t_ * 8.0f);
            c.clear(gfx::RGB(30, 4, 4));
            c.drawRect(0, 0, 16, 16, gfx::palette::Red.scaledF(0.4f + 0.6f * flash));
            gfx::drawTextCentered3x5(c, 5, "ERR!", gfx::palette::Red, 1);
            return;
        }

        c.clear(gfx::RGB(6, 10, 20));

        // Printer frame & animated gantry (top area y=0..8)
        c.drawVLine(1, 0, 9, gfx::RGB(60, 72, 92));
        c.drawVLine(14, 0, 9, gfx::RGB(60, 72, 92));
        c.drawHLine(1, 8, 14, gfx::RGB(95, 110, 130)); // Build plate

        int printedLayers = static_cast<int>(std::round((pct / 100.0f) * 5.0f));
        int nozzleY = std::max(1, 7 - printedLayers);
        int nozzleX = 7;
        if (pState == core::PrinterState::Printing) {
            nozzleX = 4 + static_cast<int>(std::round((0.5f + 0.5f * std::sin(t_ * 3.8f)) * 7.0f));
        }

        // X gantry rail
        c.drawHLine(2, nozzleY - 1, 12, gfx::RGB(72, 86, 110));

        // Printed object layers growing on build plate
        for (int row = 0; row < printedLayers; ++row) {
            int y = 7 - row;
            gfx::RGB layerCol = gfx::lerpColor(gfx::palette::Cyan, gfx::palette::SkyBlue, row / 5.0f);
            c.drawHLine(4, y, 8, layerCol);
        }

        // Hotend / extruder head
        gfx::RGB headCol = gfx::palette::Amber;
        if (pState == core::PrinterState::Heating) {
            float hPulse = 0.5f + 0.5f * std::sin(t_ * 6.0f);
            headCol = gfx::lerpColor(gfx::palette::Orange, gfx::palette::Red, hPulse);
        } else if (pState == core::PrinterState::Paused) {
            headCol = gfx::palette::Yellow;
        } else if (pState == core::PrinterState::Idle) {
            headCol = gfx::palette::Gray;
        }
        c.fillRect(nozzleX - 1, nozzleY - 1, 3, 2, headCol);
        c.setPixel(nozzleX, nozzleY + 1, gfx::palette::WarmWhite);

        // Percentage / status readout (y=10..14)
        if (pState == core::PrinterState::Heating) {
            gfx::drawTextCentered3x5(c, 10, "HEAT", gfx::palette::Orange, 1);
        } else if (pState == core::PrinterState::Paused) {
            gfx::drawTextCentered3x5(c, 10, "PAUS", gfx::palette::Yellow, 1);
        } else if (pState == core::PrinterState::Idle) {
            gfx::drawTextCentered3x5(c, 10, "IDLE", gfx::palette::Gray, 1);
        } else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(std::round(pct)));
            gfx::drawTextCentered3x5(c, 10, std::string(buf), gfx::palette::White, 1);
        }

        // Progress bar on very bottom row (y=15)
        c.drawProgressBar(0, 15, 16, pct / 100.0f,
                          (pState == core::PrinterState::Paused) ? gfx::palette::Yellow : gfx::palette::Cyan,
                          gfx::RGB(18, 26, 40));
    }

private:
    float t_ = 0.0f;
};

std::unique_ptr<DisplayMode> createPrinterMode() {
    return std::make_unique<PrinterMode>();
}

} // namespace modes
