#pragma once

#include <cstdint>
#include "core/app_state.h"
#include "core/display_engine.h"

namespace net {

// Bi-directional Home Assistant integration:
//  1) MQTT Auto-Discovery: exposes Power, Brightness, Display Mode, Playlist Selection,
//     Playlist Loop switch, Animation Speed, plus local sensor telemetry to HA.
//  2) HA REST API Poller: polls weather.*, media_player.*, 3D printer progress/state,
//     and air quality / temperature / heart rate / motion sensors directly from HA.
class HomeAssistantClient {
public:
    void begin(core::AppState& state, core::DisplayEngine& engine);
    void poll(uint32_t nowMs, core::AppState& state, core::DisplayEngine& engine);
    void publishState(const core::AppState& state);

private:
    uint32_t lastMqttRetryMs_ = 0;
    uint32_t lastStatePubMs_ = 0;
    uint32_t lastRestPollMs_ = 0;
    uint8_t restPollStep_ = 0;

    void publishDiscovery(const core::AppState& state);
    void pollNextHaEntity(core::AppState& state);
    static core::WeatherCondition parseWeatherCondition(const char* condStr);
    static core::PrinterState parsePrinterState(const char* stateStr);
};

} // namespace net
