#include "net/config_store.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#endif

namespace net {

bool ConfigStore::begin() {
#ifdef ARDUINO
    mounted_ = LittleFS.begin(true);
    if (mounted_ && !LittleFS.exists("/media")) {
        LittleFS.mkdir("/media");
    }
    return mounted_;
#else
    mounted_ = false;
    return false;
#endif
}

bool ConfigStore::loadConfig(core::AppState& state) {
#ifdef ARDUINO
    if (!mounted_) return false;
    if (!LittleFS.exists("/config.json")) {
        Serial.println("[ConfigStore] No /config.json found; writing default configuration...");
        saveConfig(state);
        return false;
    }
    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) {
        f.close();
        return false;
    }
    f.close();

    state.powerOn = doc["powerOn"] | state.powerOn;
    state.brightness = doc["brightness"] | state.brightness;
    state.animationSpeed = doc["animationSpeed"] | state.animationSpeed;
    state.primaryColor = gfx::RGB::fromHex(doc["primaryColor"] | state.primaryColor.toHex());
    state.accentColor = gfx::RGB::fromHex(doc["accentColor"] | state.accentColor.toHex());
    state.transition = gfx::transitionFromName(doc["transition"] | "crossfade");
    state.transitionMs = doc["transitionMs"] | state.transitionMs;
    state.activeMode = modes::modeFromKey(doc["activeMode"] | "clock");
    state.playlistPlaying = doc["playlistPlaying"] | false;
    state.activePlaylistId = doc["activePlaylistId"] | "default";

    bool migrated = false;
    if (doc["network"].is<JsonObject>()) {
        JsonObject n = doc["network"];
        std::string savedSsid = n["wifiSsid"] | "";
        std::string savedPass = n["wifiPassword"] | "";
        if (savedSsid.empty() && std::string(DEFAULT_WIFI_SSID).size() > 0) {
            state.network.wifiSsid = DEFAULT_WIFI_SSID;
            state.network.wifiPassword = DEFAULT_WIFI_PASS;
            migrated = true;
        } else {
            state.network.wifiSsid = savedSsid;
            state.network.wifiPassword = savedPass;
        }
        state.network.hostname = n["hostname"] | "matrix-acrylic";
        state.network.haEnabled = n["haEnabled"] | false;
        state.network.haUrl = n["haUrl"] | state.network.haUrl;
        state.network.haToken = n["haToken"] | "";
        state.network.mqttEnabled = n["mqttEnabled"] | false;
        state.network.mqttHost = n["mqttHost"] | state.network.mqttHost;
        state.network.mqttPort = n["mqttPort"] | 1883;
        state.network.mqttUser = n["mqttUser"] | "";
        state.network.mqttPassword = n["mqttPassword"] | "";
        state.network.mqttPrefix = n["mqttPrefix"] | "matrix_acrylic";
        state.network.haWeatherEntity = n["haWeatherEntity"] | state.network.haWeatherEntity;
        state.network.haMediaEntity = n["haMediaEntity"] | state.network.haMediaEntity;
        state.network.haPrinterProgressEntity = n["haPrinterProgressEntity"] | state.network.haPrinterProgressEntity;
        state.network.haPrinterStateEntity = n["haPrinterStateEntity"] | state.network.haPrinterStateEntity;
        state.network.haTempEntity = n["haTempEntity"] | state.network.haTempEntity;
        state.network.haHumidityEntity = n["haHumidityEntity"] | state.network.haHumidityEntity;
        state.network.haAqiEntity = n["haAqiEntity"] | state.network.haAqiEntity;
        state.network.haHeartRateEntity = n["haHeartRateEntity"] | state.network.haHeartRateEntity;
        state.network.haMotionEntity = n["haMotionEntity"] | state.network.haMotionEntity;
    }

    if (doc["location"].is<JsonObject>()) {
        JsonObject l = doc["location"];
        state.location.zipCode = l["zipCode"] | state.location.zipCode;
        state.location.locationLabel = l["locationLabel"] | state.location.locationLabel;
        state.location.latitude = l["latitude"] | state.location.latitude;
        state.location.longitude = l["longitude"] | state.location.longitude;
        state.location.timezone = l["timezone"] | state.location.timezone;
        state.location.tzOffsetMinutes = l["tzOffsetMinutes"] | state.location.tzOffsetMinutes;
        state.location.useFahrenheit = l["useFahrenheit"] | state.location.useFahrenheit;
    }

    if (doc["matrixHw"].is<JsonObject>()) {
        JsonObject mh = doc["matrixHw"];
        state.matrixHw.serpentine = mh["serpentine"] | state.matrixHw.serpentine;
        state.matrixHw.rotation = mh["rotation"] | state.matrixHw.rotation;
        state.matrixHw.flipX = mh["flipX"] | state.matrixHw.flipX;
        state.matrixHw.flipY = mh["flipY"] | state.matrixHw.flipY;
    }

    if (doc["nightSchedule"].is<JsonObject>()) {
        JsonObject ns = doc["nightSchedule"];
        state.nightSchedule.dimEnabled = ns["dimEnabled"] | state.nightSchedule.dimEnabled;
        state.nightSchedule.dimStartHour = ns["dimStartHour"] | 22;
        state.nightSchedule.dimStartMinute = ns["dimStartMinute"] | 0;
        state.nightSchedule.dimEndHour = ns["dimEndHour"] | 7;
        state.nightSchedule.dimEndMinute = ns["dimEndMinute"] | 0;
        state.nightSchedule.dimBrightness = ns["dimBrightness"] | 40;
        state.nightSchedule.autoOffEnabled = ns["autoOffEnabled"] | false;
        state.nightSchedule.offStartHour = ns["offStartHour"] | 23;
        state.nightSchedule.offStartMinute = ns["offStartMinute"] | 30;
        state.nightSchedule.offEndHour = ns["offEndHour"] | 6;
        state.nightSchedule.offEndMinute = ns["offEndMinute"] | 30;
        state.nightSchedule.wakeOnMotion = ns["wakeOnMotion"] | true;
        state.nightSchedule.wakeDurationSec = ns["wakeDurationSec"] | 60;
    }

    if (doc["modeSettings"].is<JsonObject>()) {
        JsonObject ms = doc["modeSettings"];
        state.modeSettings.clock24h = ms["clock24h"] | false;
        state.modeSettings.clockShowSecondsBar = ms["clockShowSecondsBar"] | true;
        state.modeSettings.clockColorStyle = ms["clockColorStyle"] | "gradient";
        state.modeSettings.weatherShowTemp = ms["weatherShowTemp"] | true;
        state.modeSettings.moonShowPercent = ms["moonShowPercent"] | false;
        state.modeSettings.musicStyle = ms["musicStyle"] | "spectrum";
        state.modeSettings.musicSensitivity = ms["musicSensitivity"] | 1.0f;
        state.modeSettings.seasonalTheme = ms["seasonalTheme"] | "auto";
        state.modeSettings.sunGradualEffect = ms["sunGradualEffect"] | true;
        state.modeSettings.sunGradualWindowMin = ms["sunGradualWindowMin"] | 30;
        state.modeSettings.printerAlertFlash = ms["printerAlertFlash"] | true;
        state.modeSettings.pixelArtScene = ms["pixelArtScene"] | "cabin";
        state.modeSettings.motionStyle = ms["motionStyle"] | "ripples";
        state.modeSettings.mirrorFlipX = ms["mirrorFlipX"] | true;
        state.modeSettings.mirrorPalette = ms["mirrorPalette"] | "truecolor";
        state.modeSettings.heartRateShowEcg = ms["heartRateShowEcg"] | true;
        state.modeSettings.airQualityShowCo2 = ms["airQualityShowCo2"] | true;
    }

    if (doc["playlists"].is<JsonArray>()) {
        state.playlists.clear();
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
            if (!pl.pages.empty()) state.playlists.push_back(pl);
        }
        if (state.playlists.empty()) state.initDefaults();
    }

    if (doc["schedules"].is<JsonArray>()) {
        state.schedules.clear();
        for (JsonObject so : doc["schedules"].as<JsonArray>()) {
            core::ScheduleRule r;
            r.enabled = so["enabled"] | true;
            r.hour = so["hour"] | 7;
            r.minute = so["minute"] | 0;
            r.daysMask = so["daysMask"] | 0x7F;
            r.action = so["action"] | "mode";
            r.value = so["value"] | "clock";
            r.brightness = so["brightness"] | 180;
            state.schedules.push_back(r);
        }
    }

    if (migrated) {
        Serial.println("[ConfigStore] Applied default Wi-Fi credentials; updating /config.json");
        saveConfig(state);
    }
    return true;
#else
    (void)state;
    return false;
#endif
}

bool ConfigStore::saveConfig(const core::AppState& state) {
#ifdef ARDUINO
    if (!mounted_) return false;
    JsonDocument doc;
    doc["powerOn"] = state.powerOn;
    doc["brightness"] = state.brightness;
    doc["animationSpeed"] = state.animationSpeed;
    doc["primaryColor"] = state.primaryColor.toHex();
    doc["accentColor"] = state.accentColor.toHex();
    doc["transition"] = gfx::transitionName(state.transition);
    doc["transitionMs"] = state.transitionMs;
    doc["activeMode"] = modes::getModeInfo(state.activeMode).key;
    doc["playlistPlaying"] = state.playlistPlaying;
    doc["activePlaylistId"] = state.activePlaylistId;

    JsonObject n = doc["network"].to<JsonObject>();
    n["wifiSsid"] = state.network.wifiSsid;
    n["wifiPassword"] = state.network.wifiPassword;
    n["hostname"] = state.network.hostname;
    n["haEnabled"] = state.network.haEnabled;
    n["haUrl"] = state.network.haUrl;
    n["haToken"] = state.network.haToken;
    n["mqttEnabled"] = state.network.mqttEnabled;
    n["mqttHost"] = state.network.mqttHost;
    n["mqttPort"] = state.network.mqttPort;
    n["mqttUser"] = state.network.mqttUser;
    n["mqttPassword"] = state.network.mqttPassword;
    n["mqttPrefix"] = state.network.mqttPrefix;
    n["haWeatherEntity"] = state.network.haWeatherEntity;
    n["haMediaEntity"] = state.network.haMediaEntity;
    n["haPrinterProgressEntity"] = state.network.haPrinterProgressEntity;
    n["haPrinterStateEntity"] = state.network.haPrinterStateEntity;
    n["haTempEntity"] = state.network.haTempEntity;
    n["haHumidityEntity"] = state.network.haHumidityEntity;
    n["haAqiEntity"] = state.network.haAqiEntity;
    n["haHeartRateEntity"] = state.network.haHeartRateEntity;
    n["haMotionEntity"] = state.network.haMotionEntity;

    JsonObject l = doc["location"].to<JsonObject>();
    l["zipCode"] = state.location.zipCode;
    l["locationLabel"] = state.location.locationLabel;
    l["latitude"] = state.location.latitude;
    l["longitude"] = state.location.longitude;
    l["timezone"] = state.location.timezone;
    l["tzOffsetMinutes"] = state.location.tzOffsetMinutes;
    l["useFahrenheit"] = state.location.useFahrenheit;

    JsonObject mh = doc["matrixHw"].to<JsonObject>();
    mh["serpentine"] = state.matrixHw.serpentine;
    mh["rotation"] = state.matrixHw.rotation;
    mh["flipX"] = state.matrixHw.flipX;
    mh["flipY"] = state.matrixHw.flipY;

    JsonObject ns = doc["nightSchedule"].to<JsonObject>();
    ns["dimEnabled"] = state.nightSchedule.dimEnabled;
    ns["dimStartHour"] = state.nightSchedule.dimStartHour;
    ns["dimStartMinute"] = state.nightSchedule.dimStartMinute;
    ns["dimEndHour"] = state.nightSchedule.dimEndHour;
    ns["dimEndMinute"] = state.nightSchedule.dimEndMinute;
    ns["dimBrightness"] = state.nightSchedule.dimBrightness;
    ns["autoOffEnabled"] = state.nightSchedule.autoOffEnabled;
    ns["offStartHour"] = state.nightSchedule.offStartHour;
    ns["offStartMinute"] = state.nightSchedule.offStartMinute;
    ns["offEndHour"] = state.nightSchedule.offEndHour;
    ns["offEndMinute"] = state.nightSchedule.offEndMinute;
    ns["wakeOnMotion"] = state.nightSchedule.wakeOnMotion;
    ns["wakeDurationSec"] = state.nightSchedule.wakeDurationSec;

    JsonObject ms = doc["modeSettings"].to<JsonObject>();
    ms["clock24h"] = state.modeSettings.clock24h;
    ms["clockShowSecondsBar"] = state.modeSettings.clockShowSecondsBar;
    ms["clockColorStyle"] = state.modeSettings.clockColorStyle;
    ms["weatherShowTemp"] = state.modeSettings.weatherShowTemp;
    ms["moonShowPercent"] = state.modeSettings.moonShowPercent;
    ms["musicStyle"] = state.modeSettings.musicStyle;
    ms["musicSensitivity"] = state.modeSettings.musicSensitivity;
    ms["seasonalTheme"] = state.modeSettings.seasonalTheme;
    ms["sunGradualEffect"] = state.modeSettings.sunGradualEffect;
    ms["sunGradualWindowMin"] = state.modeSettings.sunGradualWindowMin;
    ms["printerAlertFlash"] = state.modeSettings.printerAlertFlash;
    ms["pixelArtScene"] = state.modeSettings.pixelArtScene;
    ms["motionStyle"] = state.modeSettings.motionStyle;
    ms["mirrorFlipX"] = state.modeSettings.mirrorFlipX;
    ms["mirrorPalette"] = state.modeSettings.mirrorPalette;
    ms["heartRateShowEcg"] = state.modeSettings.heartRateShowEcg;
    ms["airQualityShowCo2"] = state.modeSettings.airQualityShowCo2;

    JsonArray pls = doc["playlists"].to<JsonArray>();
    for (const auto& pl : state.playlists) {
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

    JsonArray sch = doc["schedules"].to<JsonArray>();
    for (const auto& r : state.schedules) {
        JsonObject so = sch.add<JsonObject>();
        so["enabled"] = r.enabled;
        so["hour"] = r.hour;
        so["minute"] = r.minute;
        so["daysMask"] = r.daysMask;
        so["action"] = r.action;
        so["value"] = r.value;
        so["brightness"] = r.brightness;
    }

    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
#else
    (void)state;
    return false;
#endif
}

bool ConfigStore::loadAllMedia(core::MediaStore& media) {
#ifdef ARDUINO
    if (!mounted_) return false;
    File dir = LittleFS.open("/media");
    if (!dir || !dir.isDirectory()) return false;
    File entry = dir.openNextFile();
    while (entry) {
        std::string name = entry.name();
        if (name.size() > 4 && name.substr(name.size() - 4) == ".mpx") {
            std::vector<uint8_t> buf(entry.size());
            entry.read(buf.data(), buf.size());
            // Strip path & extension for id
            size_t slash = name.find_last_of('/');
            std::string base = (slash == std::string::npos) ? name : name.substr(slash + 1);
            std::string id = base.substr(0, base.size() - 4);
            media.putMpx(id, id, buf.data(), buf.size());
        }
        entry.close();
        entry = dir.openNextFile();
    }
    return true;
#else
    (void)media;
    return false;
#endif
}

bool ConfigStore::saveMediaFile(const std::string& id, const uint8_t* data, size_t len) {
#ifdef ARDUINO
    if (!mounted_) return false;
    std::string path = "/media/" + id + ".mpx";
    File f = LittleFS.open(path.c_str(), "w");
    if (!f) return false;
    f.write(data, len);
    f.close();
    return true;
#else
    (void)id; (void)data; (void)len;
    return false;
#endif
}

bool ConfigStore::deleteMediaFile(const std::string& id) {
#ifdef ARDUINO
    if (!mounted_) return false;
    std::string path = "/media/" + id + ".mpx";
    return LittleFS.remove(path.c_str());
#else
    (void)id;
    return false;
#endif
}

} // namespace net
