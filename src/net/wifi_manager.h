#pragma once

#include <cstdint>
#include "core/app_state.h"

namespace net {

// Non-blocking Wi-Fi, NTP, and ZIP-code geocoding manager.
// Connects to configured Wi-Fi (default: "iot") first; automatically enables the
// "MatrixDisplay-Setup" SoftAP fallback if unreachable after 12 seconds, and shuts
// the fallback AP down automatically once Wi-Fi connects.
class WifiManager {
public:
    void begin(core::AppState& state);
    void poll(uint32_t nowMs, core::AppState& state);
    void reconnect(core::AppState& state);

    // Schedule a non-blocking reconnect after sending an HTTP response
    void scheduleReconnect(uint32_t delayMs = 600);
    void requestLocationRefresh();

private:
    uint32_t connectStartMs_ = 0;
    uint32_t lastReconnectAttemptMs_ = 0;
    uint32_t lastTimeTickMs_ = 0;
    uint32_t pendingReconnectAtMs_ = 0;
    uint32_t lastWeatherFetchMs_ = 0;
    bool pendingReconnect_ = false;
    bool needZipLookup_ = true;
    bool ntpConfigured_ = false;
    bool mdnsStarted_ = false;
    bool wasConnected_ = false;

    void startFallbackAp(core::AppState& state);
    void resolveZipOnline(core::AppState& state);
    void fetchOpenMeteoWeather(core::AppState& state);
};

} // namespace net
