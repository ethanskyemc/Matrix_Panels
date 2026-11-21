#include "net/ha_client.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#endif

namespace net {

#ifdef ARDUINO
static WiFiClient gMqttNetClient;
static PubSubClient gMqtt(gMqttNetClient);
static core::AppState* gBoundState = nullptr;
static core::DisplayEngine* gBoundEngine = nullptr;

static void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
    if (!gBoundState || !gBoundEngine) return;
    std::string t(topic);
    std::string msg(reinterpret_cast<const char*>(payload), length);
    const std::string& pfx = gBoundState->network.mqttPrefix;

    if (t == pfx + "/cmd/power") {
        gBoundState->powerOn = (msg == "ON" || msg == "1" || msg == "true");
    } else if (t == pfx + "/cmd/brightness") {
        int b = std::atoi(msg.c_str());
        gBoundState->brightness = static_cast<uint8_t>(std::clamp(b, 0, 255));
    } else if (t == pfx + "/cmd/mode") {
        gBoundState->playlistPlaying = false;
        gBoundEngine->setMode(modes::modeFromKey(msg.c_str()), *gBoundState, true);
    } else if (t == pfx + "/cmd/playlist") {
        if (gBoundState->findPlaylist(msg) != nullptr) {
            gBoundState->activePlaylistId = msg;
            gBoundState->playlistPlaying = true;
            gBoundState->currentPlaylistIndex = 0;
            gBoundEngine->scheduler().resetPageTimer();
        }
    } else if (t == pfx + "/cmd/playlist_play") {
        gBoundState->playlistPlaying = (msg == "ON" || msg == "1" || msg == "true");
    } else if (t == pfx + "/cmd/speed") {
        float s = std::atof(msg.c_str());
        gBoundState->animationSpeed = std::clamp(s, 0.25f, 3.0f);
    } else if (t == pfx + "/cmd/light") {
        // JSON light schema from HA MQTT light entity
        JsonDocument doc;
        if (deserializeJson(doc, msg) == DeserializationError::Ok) {
            if (doc["state"].is<const char*>()) {
                gBoundState->powerOn = (std::strcmp(doc["state"], "ON") == 0);
            }
            if (doc["brightness"].is<int>()) {
                gBoundState->brightness = static_cast<uint8_t>(std::clamp<int>(doc["brightness"], 0, 255));
            }
            if (doc["effect"].is<const char*>()) {
                gBoundState->playlistPlaying = false;
                gBoundEngine->setMode(modes::modeFromKey(doc["effect"]), *gBoundState, true);
            }
        }
    }
}
#endif

core::WeatherCondition HomeAssistantClient::parseWeatherCondition(const char* s) {
    if (!s) return core::WeatherCondition::PartlyCloudy;
    std::string c(s);
    if (c.find("lightning") != std::string::npos || c.find("storm") != std::string::npos)
        return core::WeatherCondition::Storm;
    if (c.find("snow") != std::string::npos || c.find("hail") != std::string::npos)
        return core::WeatherCondition::Snow;
    if (c.find("rain") != std::string::npos || c.find("pouring") != std::string::npos)
        return core::WeatherCondition::Rain;
    if (c.find("fog") != std::string::npos)
        return core::WeatherCondition::Fog;
    if (c.find("wind") != std::string::npos)
        return core::WeatherCondition::Windy;
    if (c.find("partly") != std::string::npos)
        return core::WeatherCondition::PartlyCloudy;
    if (c.find("cloud") != std::string::npos)
        return core::WeatherCondition::Cloudy;
    if (c.find("night") != std::string::npos)
        return core::WeatherCondition::ClearNight;
    return core::WeatherCondition::ClearDay;
}

core::PrinterState HomeAssistantClient::parsePrinterState(const char* s) {
    if (!s) return core::PrinterState::Idle;
    std::string st(s);
    if (st.find("print") != std::string::npos || st.find("run") != std::string::npos)
        return core::PrinterState::Printing;
    if (st.find("heat") != std::string::npos || st.find("prep") != std::string::npos)
        return core::PrinterState::Heating;
    if (st.find("paus") != std::string::npos)
        return core::PrinterState::Paused;
    if (st.find("finish") != std::string::npos || st.find("complete") != std::string::npos)
        return core::PrinterState::Complete;
    if (st.find("err") != std::string::npos || st.find("fail") != std::string::npos)
        return core::PrinterState::Error;
    return core::PrinterState::Idle;
}

void HomeAssistantClient::begin(core::AppState& state, core::DisplayEngine& engine) {
#ifdef ARDUINO
    gBoundState = &state;
    gBoundEngine = &engine;
    gMqtt.setBufferSize(2048);
    gMqtt.setCallback(onMqttMessage);
#else
    (void)state; (void)engine;
#endif
}

void HomeAssistantClient::publishDiscovery(const core::AppState& state) {
#ifdef ARDUINO
    if (!gMqtt.connected()) return;
    const std::string& pfx = state.network.mqttPrefix;

    auto addDevice = [&](JsonDocument& doc) {
        JsonObject dev = doc["device"].to<JsonObject>();
        dev["identifiers"].add("matrix_acrylic_16x16");
        dev["name"] = "Matrix Acrylic Display";
        dev["model"] = "16x16 Smart LED Acrylic Panel";
        dev["manufacturer"] = "DIY Matrix";
        dev["sw_version"] = FW_VERSION;
    };

    // 1. Light entity (Power + Brightness + Effect=Mode)
    {
        JsonDocument doc;
        doc["name"] = "Matrix Display";
        doc["unique_id"] = "matrix_acrylic_light";
        doc["schema"] = "json";
        doc["state_topic"] = pfx + "/state/light";
        doc["command_topic"] = pfx + "/cmd/light";
        doc["brightness"] = true;
        doc["effect"] = true;
        JsonArray eff = doc["effect_list"].to<JsonArray>();
        for (int i = 0; i < modes::kModeCount; ++i) {
            eff.add(modes::getModeInfo(static_cast<modes::ModeId>(i)).key);
        }
        addDevice(doc);
        String out;
        serializeJson(doc, out);
        gMqtt.publish("homeassistant/light/matrix_acrylic/config", out.c_str(), true);
    }

    // 2. Select entity: Display Mode
    {
        JsonDocument doc;
        doc["name"] = "Display Mode";
        doc["unique_id"] = "matrix_acrylic_mode";
        doc["state_topic"] = pfx + "/state/mode";
        doc["command_topic"] = pfx + "/cmd/mode";
        JsonArray opts = doc["options"].to<JsonArray>();
        for (int i = 0; i < modes::kModeCount; ++i) {
            opts.add(modes::getModeInfo(static_cast<modes::ModeId>(i)).key);
        }
        addDevice(doc);
        String out;
        serializeJson(doc, out);
        gMqtt.publish("homeassistant/select/matrix_acrylic_mode/config", out.c_str(), true);
    }

    // 3. Select entity: Playlist
    {
        JsonDocument doc;
        doc["name"] = "Playlist";
        doc["unique_id"] = "matrix_acrylic_playlist";
        doc["state_topic"] = pfx + "/state/playlist";
        doc["command_topic"] = pfx + "/cmd/playlist";
        JsonArray opts = doc["options"].to<JsonArray>();
        for (const auto& pl : state.playlists) {
            opts.add(pl.id);
        }
        addDevice(doc);
        String out;
        serializeJson(doc, out);
        gMqtt.publish("homeassistant/select/matrix_acrylic_playlist/config", out.c_str(), true);
    }

    // 4. Switch entity: Playlist Loop
    {
        JsonDocument doc;
        doc["name"] = "Playlist Loop";
        doc["unique_id"] = "matrix_acrylic_playlist_loop";
        doc["state_topic"] = pfx + "/state/playlist_play";
        doc["command_topic"] = pfx + "/cmd/playlist_play";
        addDevice(doc);
        String out;
        serializeJson(doc, out);
        gMqtt.publish("homeassistant/switch/matrix_acrylic_pl/config", out.c_str(), true);
    }

    // 5. Telemetry Sensors (Heart Rate, Indoor Temp, AQI)
    auto pubSensor = [&](const char* uid, const char* name, const char* subTopic, const char* unit, const char* devClass) {
        JsonDocument doc;
        doc["name"] = name;
        doc["unique_id"] = uid;
        doc["state_topic"] = pfx + "/sensor/" + subTopic;
        if (unit && unit[0]) doc["unit_of_measurement"] = unit;
        if (devClass && devClass[0]) doc["device_class"] = devClass;
        addDevice(doc);
        String out;
        serializeJson(doc, out);
        std::string cfgTopic = std::string("homeassistant/sensor/") + uid + "/config";
        gMqtt.publish(cfgTopic.c_str(), out.c_str(), true);
    };
    pubSensor("matrix_acrylic_hr",   "Heart Rate",  "heart_rate", "bpm", "");
    pubSensor("matrix_acrylic_temp", "Temperature", "temp",       "°C",  "temperature");
    pubSensor("matrix_acrylic_aqi",  "Air Quality", "aqi",        "AQI", "aqi");
#else
    (void)state;
#endif
}

void HomeAssistantClient::publishState(const core::AppState& state) {
#ifdef ARDUINO
    if (!gMqtt.connected()) return;
    const std::string& pfx = state.network.mqttPrefix;

    JsonDocument lightDoc;
    lightDoc["state"] = state.powerOn ? "ON" : "OFF";
    lightDoc["brightness"] = state.brightness;
    lightDoc["effect"] = modes::getModeInfo(state.activeMode).key;
    String lightStr;
    serializeJson(lightDoc, lightStr);
    gMqtt.publish((pfx + "/state/light").c_str(), lightStr.c_str(), true);

    gMqtt.publish((pfx + "/state/mode").c_str(), modes::getModeInfo(state.activeMode).key, true);
    gMqtt.publish((pfx + "/state/playlist").c_str(), state.activePlaylistId.c_str(), true);
    gMqtt.publish((pfx + "/state/playlist_play").c_str(), state.playlistPlaying ? "ON" : "OFF", true);

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f", state.live.heartRateBpm);
    gMqtt.publish((pfx + "/sensor/heart_rate").c_str(), buf);
    std::snprintf(buf, sizeof(buf), "%.1f", state.live.indoorTempC);
    gMqtt.publish((pfx + "/sensor/temp").c_str(), buf);
    std::snprintf(buf, sizeof(buf), "%d", state.live.aqi);
    gMqtt.publish((pfx + "/sensor/aqi").c_str(), buf);
#else
    (void)state;
#endif
}

void HomeAssistantClient::pollNextHaEntity(core::AppState& state) {
#ifdef ARDUINO
    if (!state.network.haEnabled || state.network.haUrl.empty() || state.network.haToken.empty()) return;

    auto fetchEntity = [&](const std::string& entityId, JsonDocument& outDoc) -> bool {
        if (entityId.empty()) return false;
        HTTPClient http;
        http.setTimeout(1500);
        std::string url = state.network.haUrl + "/api/states/" + entityId;
        if (!http.begin(url.c_str())) return false;
        http.addHeader("Authorization", ("Bearer " + state.network.haToken).c_str());
        http.addHeader("Content-Type", "application/json");
        int code = http.GET();
        if (code != 200) {
            http.end();
            return false;
        }
        String payload = http.getString();
        http.end();
        return deserializeJson(outDoc, payload) == DeserializationError::Ok;
    };

    // Stagger polling one entity per tick so display FPS never stutters
    JsonDocument doc;
    bool ok = false;
    switch (restPollStep_ % 6) {
        case 0: // Weather
            if ((ok = fetchEntity(state.network.haWeatherEntity, doc))) {
                state.live.weatherCondition = parseWeatherCondition(doc["state"] | "partlycloudy");
                state.live.weatherTempC = doc["attributes"]["temperature"] | state.live.weatherTempC;
                state.live.weatherHumidity = doc["attributes"]["humidity"] | state.live.weatherHumidity;
                state.live.weatherValid = true;
            }
            break;
        case 1: // Media player (now playing)
            if ((ok = fetchEntity(state.network.haMediaEntity, doc))) {
                const char* st = doc["state"] | "idle";
                state.live.mediaPlaying = (std::strcmp(st, "playing") == 0);
                state.live.mediaTitle = doc["attributes"]["media_title"] | "";
                state.live.mediaArtist = doc["attributes"]["media_artist"] | "";
            }
            break;
        case 2: // 3D Printer progress
            if ((ok = fetchEntity(state.network.haPrinterProgressEntity, doc))) {
                const char* st = doc["state"] | "0";
                state.live.printerProgress = std::atof(st);
            }
            break;
        case 3: // 3D Printer state
            if ((ok = fetchEntity(state.network.haPrinterStateEntity, doc))) {
                state.live.printerState = parsePrinterState(doc["state"] | "idle");
            }
            break;
        case 4: // Indoor temp & AQI
            if ((ok = fetchEntity(state.network.haTempEntity, doc))) {
                state.live.indoorTempC = std::atof(doc["state"] | "22");
                state.live.envValid = true;
            }
            if (fetchEntity(state.network.haAqiEntity, doc)) {
                state.live.aqi = std::atoi(doc["state"] | "25");
            }
            break;
        case 5: // Heart rate / motion fallback from HA
            if (state.sensorHw.mmwaveModel == "ha" && (ok = fetchEntity(state.network.haHeartRateEntity, doc))) {
                state.live.heartRateBpm = std::atof(doc["state"] | "70");
                state.live.vitalValid = true;
                state.live.vitalPresent = true;
            }
            if (fetchEntity(state.network.haMotionEntity, doc)) {
                bool on = (std::strcmp(doc["state"] | "off", "on") == 0);
                state.live.motionDetected = on;
                if (on) {
                    state.live.motionIntensity = 0.85f;
                    state.live.lastMotionMs = millis();
                }
            }
            break;
    }
    state.live.haConnected = ok || state.live.haConnected;
    ++restPollStep_;
#else
    (void)state;
#endif
}

void HomeAssistantClient::poll(uint32_t nowMs, core::AppState& state, core::DisplayEngine& engine) {
#ifdef ARDUINO
    if (!state.live.wifiConnected) {
        state.live.mqttConnected = false;
        state.live.haConnected = false;
        return;
    }

    // 1. MQTT Auto-Discovery & command subscription
    if (state.network.mqttEnabled && !state.network.mqttHost.empty()) {
        if (!gMqtt.connected()) {
            state.live.mqttConnected = false;
            if (nowMs - lastMqttRetryMs_ > 8000u) {
                lastMqttRetryMs_ = nowMs;
                gMqtt.setServer(state.network.mqttHost.c_str(), state.network.mqttPort);
                bool ok = state.network.mqttUser.empty()
                              ? gMqtt.connect("matrix_acrylic_16x16")
                              : gMqtt.connect("matrix_acrylic_16x16",
                                              state.network.mqttUser.c_str(),
                                              state.network.mqttPassword.c_str());
                if (ok) {
                    state.live.mqttConnected = true;
                    gMqtt.subscribe((state.network.mqttPrefix + "/cmd/#").c_str());
                    publishDiscovery(state);
                    publishState(state);
                }
            }
        } else {
            state.live.mqttConnected = true;
            gMqtt.loop();
            if (nowMs - lastStatePubMs_ > 5000u) {
                lastStatePubMs_ = nowMs;
                publishState(state);
            }
        }
    }

    // 2. HA REST API entity polling (1 entity every 5s -> full refresh every 30s)
    if (state.network.haEnabled && (nowMs - lastRestPollMs_ > 5000u)) {
        lastRestPollMs_ = nowMs;
        pollNextHaEntity(state);
    }
    (void)engine;
#else
    (void)nowMs; (void)state; (void)engine;
#endif
}

} // namespace net
