#include "hw/audio_input.h"

#include <algorithm>
#include <cmath>

#ifdef ARDUINO
#include <Arduino.h>
#include <driver/i2s.h>
#endif

namespace hw {

void AudioInput::begin(const core::SensorHardwareConfig& cfg) {
    i2sReady_ = false;
#ifdef ARDUINO
    if (!cfg.audioEnabled || cfg.audioSource != "i2s") return;

    i2s_config_t i2sCfg = {};
    i2sCfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    i2sCfg.sample_rate = 16000;
    i2sCfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2sCfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2sCfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sCfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sCfg.dma_buf_count = 4;
    i2sCfg.dma_buf_len = 128;

    i2s_pin_config_t pinCfg = {};
    pinCfg.bck_io_num = cfg.i2sSckPin;
    pinCfg.ws_io_num = cfg.i2sWsPin;
    pinCfg.data_out_num = I2S_PIN_NO_CHANGE;
    pinCfg.data_in_num = cfg.i2sSdPin;

    if (i2s_driver_install(I2S_NUM_0, &i2sCfg, 0, nullptr) == ESP_OK &&
        i2s_set_pin(I2S_NUM_0, &pinCfg) == ESP_OK) {
        i2sReady_ = true;
    }
#else
    (void)cfg;
#endif
}

void AudioInput::poll(uint32_t nowMs, core::AppState& state) {
    // If companion app is actively streaming audio bands, don't overwrite
    if (state.live.audioActive && (nowMs - state.live.lastAudioMs) < 1500u &&
        state.sensorHw.audioSource == "stream") {
        return;
    }
    if (state.live.audioActive && (nowMs - state.live.lastAudioMs) > 2500u) {
        state.live.audioActive = false;
    }

#ifdef ARDUINO
    if (!state.sensorHw.audioEnabled || !i2sReady_) return;
    if (nowMs - lastSampleMs_ < 33u) return;
    lastSampleMs_ = nowMs;

    static constexpr int kN = 128;
    int32_t raw[kN];
    size_t bytesRead = 0;
    if (i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytesRead, 0) != ESP_OK || bytesRead < sizeof(raw)) {
        return;
    }

    float samples[kN];
    float mean = 0.0f;
    for (int i = 0; i < kN; ++i) {
        samples[i] = static_cast<float>(raw[i] >> 14) / 131072.0f;
        mean += samples[i];
    }
    mean /= kN;
    float rms = 0.0f;
    for (int i = 0; i < kN; ++i) {
        samples[i] -= mean;
        rms += samples[i] * samples[i];
    }
    rms = std::sqrt(rms / kN);

    // 16 logarithmically spaced Goertzel center frequencies (80 Hz .. 6400 Hz)
    static constexpr float kFreqs[16] = {
        80, 115, 160, 220, 300, 410, 560, 760,
        1020, 1380, 1860, 2500, 3300, 4200, 5200, 6400
    };
    const float fs = 16000.0f;
    const float gain = state.sensorHw.micGain * 4.5f;

    for (int b = 0; b < 16; ++b) {
        float omega = 2.0f * 3.14159265f * kFreqs[b] / fs;
        float coeff = 2.0f * std::cos(omega);
        float s0 = 0, s1 = 0, s2 = 0;
        for (int i = 0; i < kN; ++i) {
            s0 = samples[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        float mag = std::sqrt(std::max(0.0f, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (kN * 0.5f);
        state.live.audioBands[b] = std::clamp((mag - noiseFloor_) * gain, 0.0f, 1.0f);
    }

    noiseFloor_ = noiseFloor_ * 0.995f + std::min(rms, 0.04f) * 0.005f;
    state.live.audioLevel = std::clamp(rms * gain, 0.0f, 1.0f);
    if (state.live.audioLevel > 0.03f) {
        state.live.audioActive = true;
        state.live.lastAudioMs = nowMs;
    }
#else
    (void)nowMs; (void)state;
#endif
}

} // namespace hw
