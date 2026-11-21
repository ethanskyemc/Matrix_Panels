#pragma once

#include <cstdint>
#include "core/app_state.h"
#include "gfx/canvas.h"

namespace hw {

class MatrixDriver {
public:
    void begin(const core::MatrixHardwareConfig& cfg);
    void applyConfig(const core::MatrixHardwareConfig& cfg);
    void show(const gfx::Canvas& canvas, uint8_t brightness, bool powerOn);

    // Compute physical LED strip index from logical (x, y)
    static uint16_t mapXY(int x, int y, const core::MatrixHardwareConfig& cfg);

private:
    core::MatrixHardwareConfig cfg_;
    bool initialized_ = false;
};

} // namespace hw
