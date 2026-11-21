#pragma once

#include <cstdint>
#include "core/app_state.h"

namespace hw {

// Supports Seeed Studio MR60BHA1 / MR60BHA2 60GHz Vital-Sign mmWave Radar
// (Heart rate BPM, respiratory rate, presence) and HLK-LD2410 presence radar.
class MmWaveSensor {
public:
    void begin(const core::SensorHardwareConfig& cfg);
    void poll(uint32_t nowMs, core::AppState& state);

private:
    bool started_ = false;
    uint8_t buf_[64]{};
    size_t pos_ = 0;

    void parseMr60Frame(const uint8_t* frame, size_t len, uint32_t nowMs, core::AppState& state);
};

} // namespace hw
