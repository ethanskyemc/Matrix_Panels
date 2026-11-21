#include "net/web_server.h"

#include <algorithm>
#include <cstring>
#include "core/astro.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#endif

namespace net {

#ifdef ARDUINO
static WebServer gHttp(80);
static WebSocketsServer gWs(81);

static core::AppState* gState = nullptr;
static core::DisplayEngine* gEngine = nullptr;
static core::MediaStore* gMedia = nullptr;
static ConfigStore* gStore = nullptr;
static WifiManager* gWifi = nullptr;

static String contentTypeFor(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".png"))  return "image/png";
    return "application/octet-stream";
}

static String buildStateJson() {
    JsonDocument doc;
    doc["powerOn"] = gState->powerOn;
    doc["effectivePowerOn"] = gState->effectivePowerOn;
    doc["brightness"] = gState->brightness;
    doc["effectiveBrightness"] = gState->effectiveBrightness;
    doc["nightDimActive"] = gState->nightDimActive;
    doc["autoOffActive"] = gState->autoOffActive;
    doc["animationSpeed"] = gState->animationSpeed;
    doc["primaryColor"] = gState->primaryColor.toHex();
    doc["accentColor"] = gState->accentColor.toHex();
    doc["transition"] = gfx::transitionName(gState->transition);
    doc["transitionMs"] = gState->transitionMs;
    doc["activeMode"] = modes::getModeInfo(gState->activeMode).key;
    doc["playlistPlaying"] = gState->playlistPlaying;
    doc["activePlaylistId"] = gState->activePlaylistId;
    doc["currentPlaylistIndex"] = gState->currentPlaylistIndex;

    JsonArray mArr = doc["modes"].to<JsonArray>();
    for (int i = 0; i < modes::kModeCount; ++i) {
        const auto& info = modes::getModeInfo(static_cast<modes::ModeId>(i));
        JsonObject mo = mArr.add<JsonObject>();
        mo["key"] = info.key;
        mo["label"] = info.label;
        mo["category"] = info.category;
    }

    JsonArray mediaArr = doc["media"].to<JsonArray>();
    for (const auto* a : gMedia->list()) {
        JsonObject ao = mediaArr.add<JsonObject>();
        ao["id"] = a->id;
        ao["name"] = a->name;
        ao["animated"] = a->animated;
        ao["frames"] = a->frames.size();
    }

    JsonObject ms = doc["modeSettings"].to<JsonObject>();
    ms["clock24h"] = gState->modeSettings.clock24h;
    ms["clockShowSecondsBar"] = gState->modeSettings.clockShowSecondsBar;
    ms["clockColorStyle"] = gState->modeSettings.clockColorStyle;
    ms["weatherShowTemp"] = gState->modeSettings.weatherShowTemp;
    ms["moonShowPercent"] = gState->modeSettings.moonShowPercent;
    ms["musicStyle"] = gState->modeSettings.musicStyle;
    ms["musicSensitivity"] = gState->modeSettings.musicSensitivity;
    ms["seasonalTheme"] = gState->modeSettings.seasonalTheme;
    ms["sunGradualEffect"] = gState->modeSettings.sunGradualEffect;
    ms["sunGradualWindowMin"] = gState->modeSettings.sunGradualWindowMin;
    ms["printerAlertFlash"] = gState->modeSettings.printerAlertFlash;
    ms["pixelArtScene"] = gState->modeSettings.pixelArtScene;
    ms["motionStyle"] = gState->modeSettings.motionStyle;
    ms["mirrorFlipX"] = gState->modeSettings.mirrorFlipX;
    ms["mirrorPalette"] = gState->modeSettings.mirrorPalette;
    ms["heartRateShowEcg"] = gState->modeSettings.heartRateShowEcg;
    ms["airQualityShowCo2"] = gState->modeSettings.airQualityShowCo2;

    JsonArray pls = doc["playlists"].to<JsonArray>();
    for (const auto& pl : gState->playlists) {
        JsonObject po = pls.add<JsonObject>();
        po["id"] = pl.id;
        po["name"] = pl.name;
        JsonArray pgs = po["pages"].to<JsonArray>();
        for (const auto& pg : pl.pages) {
            JsonObject pgo = pgs.add<JsonObject>();
            pgo["mode"] = modes::getModeInfo(pg.mode).key;
            pgo["durationSec"] = pg.durationSec;
            pgo["mediaId"] = pg.mediaId;
            pgo["sceneVariant"] = pg.sceneVariant;
        }
    }

    JsonObject ns = doc["nightSchedule"].to<JsonObject>();
    ns["dimEnabled"] = gState->nightSchedule.dimEnabled;
    ns["dimStartHour"] = gState->nightSchedule.dimStartHour;
    ns["dimStartMinute"] = gState->nightSchedule.dimStartMinute;
    ns["dimEndHour"] = gState->nightSchedule.dimEndHour;
    ns["dimEndMinute"] = gState->nightSchedule.dimEndMinute;
    ns["dimBrightness"] = gState->nightSchedule.dimBrightness;
    ns["autoOffEnabled"] = gState->nightSchedule.autoOffEnabled;
    ns["offStartHour"] = gState->nightSchedule.offStartHour;
    ns["offStartMinute"] = gState->nightSchedule.offStartMinute;
    ns["offEndHour"] = gState->nightSchedule.offEndHour;
    ns["offEndMinute"] = gState->nightSchedule.offEndMinute;
    ns["wakeOnMotion"] = gState->nightSchedule.wakeOnMotion;
    ns["wakeDurationSec"] = gState->nightSchedule.wakeDurationSec;

    JsonArray sch = doc["schedules"].to<JsonArray>();
    for (const auto& r : gState->schedules) {
        JsonObject so = sch.add<JsonObject>();
        so["enabled"] = r.enabled;
        so["hour"] = r.hour;
        so["minute"] = r.minute;
        so["daysMask"] = r.daysMask;
        so["action"] = r.action;
        so["value"] = r.value;
        so["brightness"] = r.brightness;
    }

    JsonObject n = doc["network"].to<JsonObject>();
    n["wifiSsid"] = gState->network.wifiSsid;
    n["wifiHasPassword"] = !gState->network.wifiPassword.empty();
    n["hostname"] = gState->network.hostname;
    n["haEnabled"] = gState->network.haEnabled;
    n["haUrl"] = gState->network.haUrl;
    n["mqttHost"] = gState->network.mqttHost;
    n["mqttPort"] = gState->network.mqttPort;
    n["haWeatherEntity"] = gState->network.haWeatherEntity;
    n["haMediaEntity"] = gState->network.haMediaEntity;
    n["haPrinterProgressEntity"] = gState->network.haPrinterProgressEntity;
    n["haPrinterStateEntity"] = gState->network.haPrinterStateEntity;
    n["haTempEntity"] = gState->network.haTempEntity;
    n["haAqiEntity"] = gState->network.haAqiEntity;

    JsonObject loc = doc["location"].to<JsonObject>();
    loc["zipCode"] = gState->location.zipCode;
    loc["locationLabel"] = gState->location.locationLabel;
    loc["latitude"] = gState->location.latitude;
    loc["longitude"] = gState->location.longitude;
    loc["tzOffsetMinutes"] = gState->location.tzOffsetMinutes;
    loc["useFahrenheit"] = gState->location.useFahrenheit;

    JsonObject shw = doc["sensorHw"].to<JsonObject>();
    shw["audioSource"] = gState->sensorHw.audioSource;
    shw["mmwaveModel"] = gState->sensorHw.mmwaveModel;
    shw["envSensorModel"] = gState->sensorHw.envSensorModel;
    shw["cameraUrl"] = gState->sensorHw.cameraUrl;

    JsonObject mhw = doc["matrixHw"].to<JsonObject>();
    mhw["serpentine"] = gState->matrixHw.serpentine;
    mhw["rotation"] = gState->matrixHw.rotation;
    mhw["flipX"] = gState->matrixHw.flipX;
    mhw["flipY"] = gState->matrixHw.flipY;

    JsonObject live = doc["live"].to<JsonObject>();
    live["wifiConnected"] = gState->live.wifiConnected;
    live["haConnected"] = gState->live.haConnected;
    live["mqttConnected"] = gState->live.mqttConnected;
    live["ipAddress"] = gState->live.ipAddress;
    live["weatherTempC"] = gState->live.weatherTempC;
    live["weatherCondition"] = static_cast<int>(gState->live.weatherCondition);
    live["printerProgress"] = gState->live.printerProgress;
    live["printerState"] = static_cast<int>(gState->live.printerState);
    live["heartRateBpm"] = gState->live.heartRateBpm;
    live["indoorTempC"] = gState->live.indoorTempC;
    live["indoorHumidity"] = gState->live.indoorHumidity;
    live["aqi"] = gState->live.aqi;
    live["co2Ppm"] = gState->live.co2Ppm;
    live["motionDetected"] = gState->live.motionDetected;

    String out;
    serializeJson(doc, out);
    return out;
}

static void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (!gState || type != WStype_BIN || length < 2) return;
    uint8_t opcode = payload[0];
    if (opcode == 0x02 && length >= 1 + kMatrixPixels * 3) {
        // Live Mirror Mode 16x16 RGB888 frame from phone/browser camera
        std::memcpy(gState->live.mirrorRgb.data(), payload + 1, kMatrixPixels * 3);
        gState->live.mirrorFrameValid = true;
        gState->live.lastMirrorMs = millis();
    } else if (opcode == 0x03 && length >= 17) {
        // Live 16-band Audio Visualizer spectrum (0..255 per band)
        float sum = 0.0f;
        for (int i = 0; i < 16; ++i) {
            float v = static_cast<float>(payload[1 + i]) / 255.0f;
            gState->live.audioBands[i] = v;
            sum += v;
        }
        gState->live.audioLevel = sum / 16.0f;
        gState->live.audioActive = true;
        gState->live.lastAudioMs = millis();
    } else if (opcode == 0x04 && length >= 3) {
        // Interactive touch/motion trigger (x:0..255, y:0..255)
        gState->live.motionX = static_cast<float>(payload[1]) / 255.0f;
        gState->live.motionY = static_cast<float>(payload[2]) / 255.0f;
        gState->live.motionDetected = true;
        gState->live.motionIntensity = 1.0f;
        gState->live.lastMotionMs = millis();
    }
    (void)num;
}
#endif

void MatrixWebServer::begin(core::AppState& state,
                            core::DisplayEngine& engine,
                            core::MediaStore& media,
                            ConfigStore& store,
                            WifiManager& wifi) {
#ifdef ARDUINO
    gState = &state;
    gEngine = &engine;
    gMedia = &media;
    gStore = &store;
    gWifi = &wifi;

    gHttp.on("/api/state", HTTP_GET, []() {
        gHttp.send(200, "application/json", buildStateJson());
    });

    gHttp.on("/api/control", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        if (doc["powerOn"].is<bool>()) gState->powerOn = doc["powerOn"];
        if (doc["brightness"].is<int>()) gState->brightness = static_cast<uint8_t>(std::clamp<int>(doc["brightness"], 0, 255));
        if (doc["animationSpeed"].is<float>()) gState->animationSpeed = std::clamp<float>(doc["animationSpeed"], 0.25f, 3.0f);
        if (doc["primaryColor"].is<uint32_t>()) gState->primaryColor = gfx::RGB::fromHex(doc["primaryColor"]);
        if (doc["accentColor"].is<uint32_t>()) gState->accentColor = gfx::RGB::fromHex(doc["accentColor"]);
        if (doc["transition"].is<const char*>()) gState->transition = gfx::transitionFromName(doc["transition"]);
        if (doc["playlistPlaying"].is<bool>()) gState->playlistPlaying = doc["playlistPlaying"];
        if (doc["activePlaylistId"].is<const char*>()) {
            gState->activePlaylistId = doc["activePlaylistId"].as<const char*>();
            gState->currentPlaylistIndex = 0;
            gEngine->scheduler().resetPageTimer();
        }
        if (doc["activeMode"].is<const char*>()) {
            gState->playlistPlaying = false;
            gEngine->setMode(modes::modeFromKey(doc["activeMode"]), *gState, true);
        }
        if (doc["playlistAction"].is<const char*>()) {
            const char* act = doc["playlistAction"];
            if (std::strcmp(act, "next") == 0) gEngine->nextPlaylistPage(*gState);
            else if (std::strcmp(act, "prev") == 0) gEngine->prevPlaylistPage(*gState);
        }
        if (doc["modeSettings"].is<JsonObject>()) {
            JsonObject ms = doc["modeSettings"];
            if (ms["clock24h"].is<bool>()) gState->modeSettings.clock24h = ms["clock24h"];
            if (ms["clockColorStyle"].is<const char*>()) gState->modeSettings.clockColorStyle = ms["clockColorStyle"].as<const char*>();
            if (ms["musicStyle"].is<const char*>()) gState->modeSettings.musicStyle = ms["musicStyle"].as<const char*>();
            if (ms["seasonalTheme"].is<const char*>()) gState->modeSettings.seasonalTheme = ms["seasonalTheme"].as<const char*>();
            if (ms["pixelArtScene"].is<const char*>()) gState->modeSettings.pixelArtScene = ms["pixelArtScene"].as<const char*>();
            if (ms["motionStyle"].is<const char*>()) gState->modeSettings.motionStyle = ms["motionStyle"].as<const char*>();
            if (ms["mirrorPalette"].is<const char*>()) gState->modeSettings.mirrorPalette = ms["mirrorPalette"].as<const char*>();
            if (ms["mirrorFlipX"].is<bool>()) gState->modeSettings.mirrorFlipX = ms["mirrorFlipX"];
            if (ms["sunGradualEffect"].is<bool>()) gState->modeSettings.sunGradualEffect = ms["sunGradualEffect"];
            if (ms["moonShowPercent"].is<bool>()) gState->modeSettings.moonShowPercent = ms["moonShowPercent"];
        }
        gStore->saveConfig(*gState);
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    // Upload compact binary .mpx image or animated GIF (query params ?id=...&name=...)
    gHttp.on("/api/media/upload", HTTP_POST, []() {
        String id = gHttp.arg("id");
        String name = gHttp.arg("name");
        if (id.isEmpty()) id = "art_" + String(millis());
        String body = gHttp.arg("plain");
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(body.c_str());
        if (!gMedia->putMpx(id.c_str(), name.c_str(), ptr, body.length())) {
            gHttp.send(400, "application/json", "{\"error\":\"invalid mpx payload\"}");
            return;
        }
        gStore->saveMediaFile(id.c_str(), ptr, body.length());
        gState->modeSettings.pixelArtScene = id.c_str();
        gEngine->setMode(modes::ModeId::PixelArt, *gState, true);
        gHttp.send(200, "application/json", "{\"ok\":true,\"id\":\"" + id + "\"}");
    });

    gHttp.on("/api/media/delete", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        std::string id = doc["id"] | "";
        gMedia->remove(id);
        gStore->deleteMediaFile(id);
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    gHttp.on("/api/playlists", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        if (doc["playlists"].is<JsonArray>()) {
            gState->playlists.clear();
            for (JsonObject po : doc["playlists"].as<JsonArray>()) {
                core::Playlist pl;
                pl.id = po["id"] | "custom";
                pl.name = po["name"] | "Playlist";
                if (po["pages"].is<JsonArray>()) {
                    for (JsonObject pgo : po["pages"].as<JsonArray>()) {
                        core::PlaylistPage pg;
                        pg.mode = modes::modeFromKey(pgo["mode"] | "clock");
                        pg.durationSec = pgo["durationSec"] | 15;
                        pg.mediaId = pgo["mediaId"] | "";
                        pg.sceneVariant = pgo["sceneVariant"] | "";
                        pl.pages.push_back(pg);
                    }
                }
                if (!pl.pages.empty()) gState->playlists.push_back(pl);
            }
            if (gState->playlists.empty()) gState->initDefaults();
            gStore->saveConfig(*gState);
        }
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    gHttp.on("/api/schedules", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        if (doc["nightSchedule"].is<JsonObject>()) {
            JsonObject ns = doc["nightSchedule"];
            gState->nightSchedule.dimEnabled = ns["dimEnabled"] | gState->nightSchedule.dimEnabled;
            gState->nightSchedule.dimStartHour = ns["dimStartHour"] | gState->nightSchedule.dimStartHour;
            gState->nightSchedule.dimStartMinute = ns["dimStartMinute"] | gState->nightSchedule.dimStartMinute;
            gState->nightSchedule.dimEndHour = ns["dimEndHour"] | gState->nightSchedule.dimEndHour;
            gState->nightSchedule.dimEndMinute = ns["dimEndMinute"] | gState->nightSchedule.dimEndMinute;
            gState->nightSchedule.dimBrightness = ns["dimBrightness"] | gState->nightSchedule.dimBrightness;
            gState->nightSchedule.autoOffEnabled = ns["autoOffEnabled"] | gState->nightSchedule.autoOffEnabled;
            gState->nightSchedule.offStartHour = ns["offStartHour"] | gState->nightSchedule.offStartHour;
            gState->nightSchedule.offStartMinute = ns["offStartMinute"] | gState->nightSchedule.offStartMinute;
            gState->nightSchedule.offEndHour = ns["offEndHour"] | gState->nightSchedule.offEndHour;
            gState->nightSchedule.offEndMinute = ns["offEndMinute"] | gState->nightSchedule.offEndMinute;
            gState->nightSchedule.wakeOnMotion = ns["wakeOnMotion"] | gState->nightSchedule.wakeOnMotion;
            gState->nightSchedule.wakeDurationSec = ns["wakeDurationSec"] | gState->nightSchedule.wakeDurationSec;
        }
        if (doc["schedules"].is<JsonArray>()) {
            gState->schedules.clear();
            for (JsonObject so : doc["schedules"].as<JsonArray>()) {
                core::ScheduleRule r;
                r.enabled = so["enabled"] | true;
                r.hour = so["hour"] | 7;
                r.minute = so["minute"] | 0;
                r.daysMask = so["daysMask"] | 0x7F;
                r.action = so["action"] | "mode";
                r.value = so["value"] | "clock";
                r.brightness = so["brightness"] | 180;
                gState->schedules.push_back(r);
            }
        }
        gStore->saveConfig(*gState);
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    gHttp.on("/api/config", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        bool wifiChanged = false;
        bool locChanged = false;
        if (doc["network"].is<JsonObject>()) {
            JsonObject n = doc["network"];
            if (n["wifiSsid"].is<const char*>()) {
                std::string newSsid = n["wifiSsid"].as<const char*>();
                if (newSsid != gState->network.wifiSsid) {
                    gState->network.wifiSsid = newSsid;
                    wifiChanged = true;
                }
            }
            if (n["wifiPassword"].is<const char*>()) {
                std::string newPass = n["wifiPassword"].as<const char*>();
                // Only overwrite saved password if user actually typed one (or changed SSID)
                if (!newPass.empty() || wifiChanged) {
                    if (newPass != gState->network.wifiPassword) {
                        gState->network.wifiPassword = newPass;
                        wifiChanged = true;
                    }
                }
            }
            if (n["hostname"].is<const char*>()) gState->network.hostname = n["hostname"].as<const char*>();
            if (n["haEnabled"].is<bool>()) gState->network.haEnabled = n["haEnabled"];
            if (n["haUrl"].is<const char*>()) gState->network.haUrl = n["haUrl"].as<const char*>();
            if (n["haToken"].is<const char*>()) {
                std::string tok = n["haToken"].as<const char*>();
                if (!tok.empty()) gState->network.haToken = tok;
            }
            if (n["mqttEnabled"].is<bool>()) gState->network.mqttEnabled = n["mqttEnabled"];
            if (n["mqttHost"].is<const char*>()) gState->network.mqttHost = n["mqttHost"].as<const char*>();
            if (n["mqttPort"].is<int>()) gState->network.mqttPort = n["mqttPort"];
            if (n["mqttUser"].is<const char*>()) gState->network.mqttUser = n["mqttUser"].as<const char*>();
            if (n["mqttPassword"].is<const char*>()) {
                std::string mp = n["mqttPassword"].as<const char*>();
                if (!mp.empty()) gState->network.mqttPassword = mp;
            }
            if (n["haWeatherEntity"].is<const char*>()) gState->network.haWeatherEntity = n["haWeatherEntity"].as<const char*>();
            if (n["haMediaEntity"].is<const char*>()) gState->network.haMediaEntity = n["haMediaEntity"].as<const char*>();
            if (n["haPrinterProgressEntity"].is<const char*>()) gState->network.haPrinterProgressEntity = n["haPrinterProgressEntity"].as<const char*>();
            if (n["haPrinterStateEntity"].is<const char*>()) gState->network.haPrinterStateEntity = n["haPrinterStateEntity"].as<const char*>();
            if (n["haTempEntity"].is<const char*>()) gState->network.haTempEntity = n["haTempEntity"].as<const char*>();
            if (n["haAqiEntity"].is<const char*>()) gState->network.haAqiEntity = n["haAqiEntity"].as<const char*>();
            if (n["haHeartRateEntity"].is<const char*>()) gState->network.haHeartRateEntity = n["haHeartRateEntity"].as<const char*>();
        }
        if (doc["location"].is<JsonObject>()) {
            JsonObject l = doc["location"];
            if (l["zipCode"].is<const char*>()) {
                std::string newZip = l["zipCode"].as<const char*>();
                if (!newZip.empty() && newZip != gState->location.zipCode) {
                    gState->location.zipCode = newZip;
                    double lat = 0, lon = 0;
                    int tzMin = gState->location.tzOffsetMinutes;
                    std::string label;
                    if (core::lookupUsZipOffline(newZip, lat, lon, tzMin, label)) {
                        gState->location.latitude = lat;
                        gState->location.longitude = lon;
                        gState->location.locationLabel = label;
                        if (!l["tzOffsetMinutes"].is<int>()) {
                            gState->location.tzOffsetMinutes = tzMin;
                        }
                    }
                    locChanged = true;
                }
            }
            if (l["latitude"].is<double>()) gState->location.latitude = l["latitude"];
            if (l["longitude"].is<double>()) gState->location.longitude = l["longitude"];
            if (l["locationLabel"].is<const char*>()) {
                std::string lbl = l["locationLabel"].as<const char*>();
                if (!lbl.empty()) gState->location.locationLabel = lbl;
            }
            if (l["timezone"].is<const char*>()) gState->location.timezone = l["timezone"].as<const char*>();
            if (l["tzOffsetMinutes"].is<int>()) {
                int newTz = l["tzOffsetMinutes"];
                if (newTz != gState->location.tzOffsetMinutes) {
                    gState->location.tzOffsetMinutes = newTz;
                    locChanged = true;
                }
            }
            if (l["useFahrenheit"].is<bool>()) gState->location.useFahrenheit = l["useFahrenheit"];
        }
        if (doc["sensorHw"].is<JsonObject>()) {
            JsonObject s = doc["sensorHw"];
            if (s["audioSource"].is<const char*>()) gState->sensorHw.audioSource = s["audioSource"].as<const char*>();
            if (s["mmwaveModel"].is<const char*>()) gState->sensorHw.mmwaveModel = s["mmwaveModel"].as<const char*>();
            if (s["envSensorModel"].is<const char*>()) gState->sensorHw.envSensorModel = s["envSensorModel"].as<const char*>();
            if (s["pirEnabled"].is<bool>()) gState->sensorHw.pirEnabled = s["pirEnabled"];
            if (s["cameraUrl"].is<const char*>()) gState->sensorHw.cameraUrl = s["cameraUrl"].as<const char*>();
        }
        if (doc["matrixHw"].is<JsonObject>()) {
            JsonObject mh = doc["matrixHw"];
            if (mh["serpentine"].is<bool>()) gState->matrixHw.serpentine = mh["serpentine"];
            if (mh["rotation"].is<int>()) gState->matrixHw.rotation = static_cast<uint8_t>(mh["rotation"].as<int>() & 3);
            if (mh["flipX"].is<bool>()) gState->matrixHw.flipX = mh["flipX"];
            if (mh["flipY"].is<bool>()) gState->matrixHw.flipY = mh["flipY"];
        }
        gStore->saveConfig(*gState);
        gHttp.send(200, "application/json",
                   wifiChanged ? "{\"ok\":true,\"reconnecting\":true}" : "{\"ok\":true}");
        if (gWifi) {
            if (locChanged) gWifi->requestLocationRefresh();
            if (wifiChanged) gWifi->scheduleReconnect(600);
        }
    });

    gHttp.on("/api/sensors/inject", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, gHttp.arg("plain")) != DeserializationError::Ok) {
            gHttp.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        if (doc["weatherCondition"].is<int>()) {
            gState->live.weatherCondition = static_cast<core::WeatherCondition>(
                std::clamp<int>(doc["weatherCondition"], 0, 8));
        }
        if (doc["weatherTempC"].is<float>()) gState->live.weatherTempC = doc["weatherTempC"];
        if (doc["printerState"].is<int>()) {
            gState->live.printerState = static_cast<core::PrinterState>(
                std::clamp<int>(doc["printerState"], 0, 5));
        }
        if (doc["printerProgress"].is<float>()) {
            gState->live.printerProgress = std::clamp<float>(doc["printerProgress"], 0.0f, 100.0f);
        }
        if (doc["heartRateBpm"].is<float>()) {
            gState->live.heartRateBpm = doc["heartRateBpm"];
            gState->live.vitalPresent = true;
            gState->live.vitalValid = true;
        }
        if (doc["aqi"].is<int>()) gState->live.aqi = std::clamp<int>(doc["aqi"], 0, 500);
        if (doc["indoorTempC"].is<float>()) gState->live.indoorTempC = doc["indoorTempC"];
        if (doc["motionDetected"].is<bool>() && doc["motionDetected"].as<bool>()) {
            gState->live.motionDetected = true;
            gState->live.motionIntensity = 1.0f;
            gState->live.motionX = doc["motionX"] | 0.5f;
            gState->live.motionY = doc["motionY"] | 0.5f;
            gState->live.lastMotionMs = millis();
        }
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    // Push 16x16 RGB888 album art frame (768 bytes raw)
    gHttp.on("/api/album_art", HTTP_POST, []() {
        String body = gHttp.arg("plain");
        if (body.length() < kMatrixPixels * 3) {
            gHttp.send(400, "application/json", "{\"error\":\"need 768 bytes\"}");
            return;
        }
        std::memcpy(gState->live.albumArtRgb.data(), body.c_str(), kMatrixPixels * 3);
        gState->live.albumArtValid = true;
        gState->live.mediaPlaying = true;
        gHttp.send(200, "application/json", "{\"ok\":true}");
    });

    // Static PWA files from LittleFS
    gHttp.onNotFound([]() {
        String path = gHttp.uri();
        if (path == "/") path = "/index.html";
        if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            gHttp.streamFile(f, contentTypeFor(path));
            f.close();
        } else {
            gHttp.send(404, "text/plain", "Not Found");
        }
    });

    gHttp.begin();
    gWs.begin();
    gWs.onEvent(handleWsEvent);
#else
    (void)state; (void)engine; (void)media; (void)store; (void)wifi;
#endif
}

void MatrixWebServer::poll(uint32_t nowMs, const core::DisplayEngine& engine) {
#ifdef ARDUINO
    gHttp.handleClient();
    gWs.loop();

    // Stream live 16x16 RGB frame over WebSocket to phone previews at ~20 FPS
    if (gWs.connectedClients() > 0 && (nowMs - lastPreviewBroadcastMs_ >= 50u)) {
        lastPreviewBroadcastMs_ = nowMs;
        uint8_t packet[1 + kMatrixPixels * 3];
        packet[0] = 0x01; // Live preview opcode
        engine.output().copyToRGB888(packet + 1);
        gWs.broadcastBIN(packet, sizeof(packet));
    }
#else
    (void)nowMs; (void)engine;
#endif
}

} // namespace net
