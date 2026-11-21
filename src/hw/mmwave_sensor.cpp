#include "hw/mmwave_sensor.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace hw {

void MmWaveSensor::begin(const core::SensorHardwareConfig& cfg) {
    started_ = false;
    pos_ = 0;
#ifdef ARDUINO
    if (!cfg.mmwaveEnabled || cfg.mmwaveModel == "ha") return;
    uint32_t baud = (cfg.mmwaveModel == "ld2410") ? 256000 : 115200;
    Serial1.begin(baud, SERIAL_8N1, cfg.mmwaveRxPin, cfg.mmwaveTxPin);
    started_ = true;
#else
    (void)cfg;
#endif
}

void MmWaveSensor::parseMr60Frame(const uint8_t* f, size_t len, uint32_t nowMs, core::AppState& state) {
    // Seeed MR60BHA1/MR60BHA2 protocol:
    // [0..1] Header: 0x53 0x59
    // [2]    Control word (0x80=presence, 0x81=breath, 0x85=heart rate)
    // [3]    Command word
    // [4..5] Length (big-endian)
    // [6..6+L-1] Payload
    // [6+L]  Checksum (sum of bytes 0..5+L & 0xFF)
    // [7+L..8+L] Tail: 0x54 0x43
    if (len < 10) return;
    uint16_t payloadLen = (static_cast<uint16_t>(f[4]) << 8) | f[5];
    if (len < static_cast<size_t>(payloadLen + 9)) return;

    uint8_t sum = 0;
    for (size_t i = 0; i < static_cast<size_t>(6 + payloadLen); ++i) sum += f[i];
    if (sum != f[6 + payloadLen]) return;

    uint8_t ctrl = f[2];
    uint8_t cmd = f[3];
    const uint8_t* payload = f + 6;

    if (ctrl == 0x85 && cmd == 0x02 && payloadLen >= 1) {
        // Heartbeat rate value in BPM
        uint8_t bpm = payload[0];
        if (bpm >= 35 && bpm <= 220) {
            state.live.heartRateBpm = static_cast<float>(bpm);
            state.live.vitalValid = true;
            state.live.vitalPresent = true;
        }
    } else if (ctrl == 0x81 && cmd == 0x02 && payloadLen >= 1) {
        // Respiratory rate value in RPM
        state.live.breathRateRpm = static_cast<float>(payload[0]);
    } else if (ctrl == 0x80 && cmd == 0x01 && payloadLen >= 1) {
        // Presence flag
        bool present = (payload[0] != 0);
        state.live.vitalPresent = present;
        if (present) {
            state.live.motionDetected = true;
            state.live.motionIntensity = 0.75f;
            state.live.lastMotionMs = nowMs;
        }
    } else if (ctrl == 0x80 && cmd == 0x02 && payloadLen >= 1) {
        // Motion energy (0=none, 1=static, 2=active)
        state.live.motionDetected = (payload[0] >= 1);
        state.live.motionIntensity = (payload[0] == 2) ? 1.0f : (payload[0] == 1 ? 0.35f : 0.0f);
        if (payload[0] >= 1) state.live.lastMotionMs = nowMs;
    }
}

void MmWaveSensor::poll(uint32_t nowMs, core::AppState& state) {
#ifdef ARDUINO
    if (!started_) return;
    while (Serial1.available() > 0) {
        uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (pos_ == 0 && b != 0x53) continue;
        if (pos_ == 1 && b != 0x59) { pos_ = 0; continue; }
        buf_[pos_++] = b;
        if (pos_ >= sizeof(buf_)) {
            pos_ = 0;
            continue;
        }
        if (pos_ >= 10 && buf_[pos_ - 2] == 0x54 && buf_[pos_ - 1] == 0x43) {
            parseMr60Frame(buf_, pos_, nowMs, state);
            pos_ = 0;
        }
    }
#else
    (void)nowMs; (void)state;
#endif
}

} // namespace hw
