#pragma once

#include <cstdint>
#include "core/app_state.h"

namespace hw {

class EnvSensors {
public:
    void begin(const core::SensorHardwareConfig& cfg);
    void poll(uint32_t nowMs, core::AppState& state);

private:
    bool bmeOk_ = false;
    bool shtOk_ = false;
    bool scdOk_ = false;
    uint32_t lastEnvPollMs_ = 0;
};

} // namespace hw
