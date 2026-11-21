#include "net/wifi_manager.h"

#include <cstdlib>
#include <string>
#include "core/astro.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>
#endif

namespace net {

void WifiManager::begin(core::AppState& state) {
    lastTimeTickMs_ = 0;
    needZipLookup_ = true;
    // Immediately apply offline ZIP lookup if coordinates weren't resolved yet
    if (!state.location.zipCode.empty() && state.location.locationLabel.empty()) {
        double lat, lon;
        int tzMin;
        std::string label;
        if (core::lookupUsZipOffline(state.location.zipCode, lat, lon, tzMin, label)) {
            state.location.latitude = lat;
            state.location.longitude = lon;
            state.location.locationLabel = label;
        }
    }
#ifdef ARDUINO
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    reconnect(state);
#else
    (void)state;
#endif
}

void WifiManager::scheduleReconnect(uint32_t delayMs) {
#ifdef ARDUINO
    pendingReconnect_ = true;
    pendingReconnectAtMs_ = millis() + delayMs;
#else
    (void)delayMs;
#endif
}

void WifiManager::requestLocationRefresh() {
    needZipLookup_ = true;
    ntpConfigured_ = false;
    lastWeatherFetchMs_ = 0;
}

void WifiManager::startFallbackAp(core::AppState& state) {
#ifdef ARDUINO
    if (state.live.apModeActive) return;
    WiFi.mode(state.network.wifiSsid.empty() ? WIFI_AP : WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.softAP("MatrixDisplay-Setup", "matrix1616");
    state.live.apModeActive = true;
    state.live.ipAddress = WiFi.softAPIP().toString().c_str();
    Serial.printf("[WiFi] Fallback AP active: SSID=\"MatrixDisplay-Setup\" Pass=\"matrix1616\" IP=%s\n",
                  state.live.ipAddress.c_str());
#else
    (void)state;
#endif
}

void WifiManager::reconnect(core::AppState& state) {
#ifdef ARDUINO
    pendingReconnect_ = false;
    wasConnected_ = false;
    ntpConfigured_ = false;
    mdnsStarted_ = false;

    if (state.network.wifiSsid.empty()) {
        state.live.wifiConnected = false;
        state.live.apModeActive = false;
        startFallbackAp(state);
        return;
    }

    // Try pure STA connection first; if unreachable after 12s, poll() brings up fallback AP
    WiFi.softAPdisconnect(true);
    state.live.apModeActive = false;
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.setHostname(state.network.hostname.c_str());
    WiFi.begin(state.network.wifiSsid.c_str(), state.network.wifiPassword.c_str());
    connectStartMs_ = millis();
    lastReconnectAttemptMs_ = connectStartMs_;
    Serial.printf("[WiFi] Connecting to \"%s\" (fallback AP starts in 12s if unreachable)...\n",
                  state.network.wifiSsid.c_str());
#else
    (void)state;
#endif
}

void WifiManager::resolveZipOnline(core::AppState& state) {
#ifdef ARDUINO
    if (state.location.zipCode.size() < 5) return;
    std::string zip5;
    for (char c : state.location.zipCode) {
        if (c >= '0' && c <= '9' && zip5.size() < 5) zip5.push_back(c);
    }
    if (zip5.size() != 5) return;

    HTTPClient http;
    http.setTimeout(2500);
    std::string url = "http://api.zippopotam.us/us/" + zip5;
    if (http.begin(url.c_str())) {
        int code = http.GET();
        if (code == 200) {
            JsonDocument doc;
            if (deserializeJson(doc, http.getString()) == DeserializationError::Ok &&
                doc["places"].is<JsonArray>() && doc["places"].size() > 0) {
                JsonObject p = doc["places"][0];
                const char* latStr = p["latitude"] | "";
                const char* lonStr = p["longitude"] | "";
                const char* place = p["place name"] | "";
                const char* abbr = p["state abbreviation"] | "";
                if (latStr[0] && lonStr[0]) {
                    state.location.latitude = std::atof(latStr);
                    state.location.longitude = std::atof(lonStr);
                    state.location.locationLabel = std::string(place) + ", " + std::string(abbr);
                    Serial.printf("[Location] ZIP %s -> %s (%.4f, %.4f)\n",
                                  zip5.c_str(), state.location.locationLabel.c_str(),
                                  state.location.latitude, state.location.longitude);
                }
            }
        }
        http.end();
    }
#else
    (void)state;
#endif
}

void WifiManager::fetchOpenMeteoWeather(core::AppState& state) {
#ifdef ARDUINO
    HTTPClient http;
    http.setTimeout(2500);
    char url[192];
    std::snprintf(url, sizeof(url),
                  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                  "&current=temperature_2m,relative_humidity_2m,weather_code,is_day",
                  state.location.latitude, state.location.longitude);
    if (!http.begin(url)) return;
    int code = http.GET();
    if (code == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, http.getString()) == DeserializationError::Ok &&
            doc["current"].is<JsonObject>()) {
            JsonObject cur = doc["current"];
            state.live.weatherTempC = cur["temperature_2m"] | state.live.weatherTempC;
            state.live.weatherHumidity = cur["relative_humidity_2m"] | state.live.weatherHumidity;
            int wmo = cur["weather_code"] | 0;
            int isDay = cur["is_day"] | 1;
            if (wmo == 0) {
                state.live.weatherCondition = isDay ? core::WeatherCondition::ClearDay : core::WeatherCondition::ClearNight;
            } else if (wmo <= 2) {
                state.live.weatherCondition = core::WeatherCondition::PartlyCloudy;
            } else if (wmo == 3) {
                state.live.weatherCondition = core::WeatherCondition::Cloudy;
            } else if (wmo == 45 || wmo == 48) {
                state.live.weatherCondition = core::WeatherCondition::Fog;
            } else if ((wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86) {
                state.live.weatherCondition = core::WeatherCondition::Snow;
            } else if (wmo >= 95) {
                state.live.weatherCondition = core::WeatherCondition::Storm;
            } else {
                state.live.weatherCondition = core::WeatherCondition::Rain;
            }
            state.live.weatherValid = true;
            Serial.printf("[Weather] Open-Meteo: %.1f C, WMO=%d\n", state.live.weatherTempC, wmo);
        }
    }
    http.end();
#else
    (void)state;
#endif
}

void WifiManager::poll(uint32_t nowMs, core::AppState& state) {
#ifdef ARDUINO
    if (pendingReconnect_ && static_cast<int32_t>(nowMs - pendingReconnectAtMs_) >= 0) {
        reconnect(state);
        return;
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
    state.live.wifiConnected = connected;

    if (connected) {
        state.live.rssi = WiFi.RSSI();
        state.live.ipAddress = WiFi.localIP().toString().c_str();
        if (!wasConnected_) {
            wasConnected_ = true;
            Serial.printf("[WiFi] Connected to \"%s\"! IP: http://%s (or http://%s.local)\n",
                          state.network.wifiSsid.c_str(),
                          state.live.ipAddress.c_str(),
                          state.network.hostname.c_str());
        }
        if (state.live.apModeActive) {
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            state.live.apModeActive = false;
            Serial.println("[WiFi] Disabled fallback AP (connected to router)");
        }
        if (!mdnsStarted_) {
            if (MDNS.begin(state.network.hostname.c_str())) {
                MDNS.addService("http", "tcp", 80);
                mdnsStarted_ = true;
            }
        }
        if (!ntpConfigured_) {
            long gmtOffsetSec = static_cast<long>(state.location.tzOffsetMinutes) * 60L;
            configTime(gmtOffsetSec, 0, "pool.ntp.org", "time.nist.gov");
            ntpConfigured_ = true;
        }
        if (needZipLookup_) {
            needZipLookup_ = false;
            resolveZipOnline(state);
        }
        // If Home Assistant isn't enabled, pull local weather from Open-Meteo every 15 min
        if (!state.network.haEnabled &&
            (lastWeatherFetchMs_ == 0 || (nowMs - lastWeatherFetchMs_) > 900000u)) {
            lastWeatherFetchMs_ = nowMs;
            fetchOpenMeteoWeather(state);
        }
    } else {
        wasConnected_ = false;
        if (!state.network.wifiSsid.empty()) {
            // If initial STA attempt hasn't connected after 12s, bring up fallback AP
            if (!state.live.apModeActive && (nowMs - connectStartMs_) > 12000u) {
                Serial.printf("[WiFi] Could not connect to \"%s\" within 12s; starting fallback AP...\n",
                              state.network.wifiSsid.c_str());
                startFallbackAp(state);
            }
            // Retry configured Wi-Fi every 30s in the background while keeping AP usable
            if ((nowMs - lastReconnectAttemptMs_) > 30000u) {
                lastReconnectAttemptMs_ = nowMs;
                Serial.printf("[WiFi] Retrying \"%s\"...\n", state.network.wifiSsid.c_str());
                WiFi.begin(state.network.wifiSsid.c_str(), state.network.wifiPassword.c_str());
            }
        }
    }

    // Update LocalTime (from NTP/RTC when synced, or monotonic offline fallback!)
    if (nowMs - lastTimeTickMs_ >= 1000u) {
        uint32_t elapsedSec = (nowMs - lastTimeTickMs_) / 1000u;
        lastTimeTickMs_ += elapsedSec * 1000u;

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0) && timeinfo.tm_year + 1900 >= 2024) {
            state.live.time.year = timeinfo.tm_year + 1900;
            state.live.time.month = timeinfo.tm_mon + 1;
            state.live.time.day = timeinfo.tm_mday;
            state.live.time.hour = timeinfo.tm_hour;
            state.live.time.minute = timeinfo.tm_min;
            state.live.time.second = timeinfo.tm_sec;
            state.live.time.weekday = timeinfo.tm_wday;
            state.live.time.unixEpoch = static_cast<int64_t>(time(nullptr));
            state.live.time.synced = true;
        } else {
            state.live.time.unixEpoch += elapsedSec;
            state.live.time.second += static_cast<int>(elapsedSec);
            while (state.live.time.second >= 60) {
                state.live.time.second -= 60;
                state.live.time.minute += 1;
                if (state.live.time.minute >= 60) {
                    state.live.time.minute = 0;
                    state.live.time.hour = (state.live.time.hour + 1) % 24;
                    if (state.live.time.hour == 0) {
                        state.live.time.weekday = (state.live.time.weekday + 1) % 7;
                    }
                }
            }
        }
    }
#else
    (void)nowMs; (void)state;
#endif
}

} // namespace net
