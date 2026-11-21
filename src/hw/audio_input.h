#pragma once

#include <cstdint>
#include "core/app_state.h"

namespace hw {

class AudioInput {
public:
    void begin(const core::SensorHardwareConfig& cfg);
    void poll(uint32_t nowMs, core::AppState& state);

private:
    bool i2sReady_ = false;
    float noiseFloor_ = 0.02f;
    uint32_t lastSampleMs_ = 0;
};

} // namespace hw
