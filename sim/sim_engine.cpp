// Native C++17 simulator host for the Smart 16x16 Acrylic LED Matrix Engine.
// Reads single-line commands on stdin and outputs JSON responses / live RGB frames on stdout.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/app_state.h"
#include "core/astro.h"
#include "core/display_engine.h"
#include "core/media_store.h"

namespace {

static const char kHex[] = "0123456789abcdef";

std::string bytesToHex(const uint8_t* data, size_t len) {
    std::string out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = kHex[data[i] >> 4];
        out[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((nib(hex[i]) << 4) | nib(hex[i + 1])));
    }
    return out;
}

std::string jsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out.push_back(c);
    }
    return out;
}

void updateSystemClock(core::AppState& state) {
    std::time_t now = std::time(nullptr);
    std::tm lt{};
    localtime_r(&now, &lt);
    state.live.time.year = lt.tm_year + 1900;
    state.live.time.month = lt.tm_mon + 1;
    state.live.time.day = lt.tm_mday;
    state.live.time.hour = lt.tm_hour;
    state.live.time.minute = lt.tm_min;
    state.live.time.second = lt.tm_sec;
    state.live.time.weekday = lt.tm_wday;
    state.live.time.unixEpoch = static_cast<int64_t>(now);
    state.live.time.synced = true;
}

std::string serializeStateJson(const core::AppState& s, const core::MediaStore& media) {
    std::ostringstream o;
    o << "{";
    o << "\"powerOn\":" << (s.powerOn ? "true" : "false") << ",";
    o << "\"effectivePowerOn\":" << (s.effectivePowerOn ? "true" : "false") << ",";
    o << "\"brightness\":" << static_cast<int>(s.brightness) << ",";
    o << "\"effectiveBrightness\":" << static_cast<int>(s.effectiveBrightness) << ",";
    o << "\"nightDimActive\":" << (s.nightDimActive ? "true" : "false") << ",";
    o << "\"autoOffActive\":" << (s.autoOffActive ? "true" : "false") << ",";
    o << "\"animationSpeed\":" << s.animationSpeed << ",";
    o << "\"primaryColor\":" << s.primaryColor.toHex() << ",";
    o << "\"accentColor\":" << s.accentColor.toHex() << ",";
    o << "\"transition\":\"" << gfx::transitionName(s.transition) << "\",";
    o << "\"transitionMs\":" << s.transitionMs << ",";
    o << "\"activeMode\":\"" << modes::getModeInfo(s.activeMode).key << "\",";
    o << "\"playlistPlaying\":" << (s.playlistPlaying ? "true" : "false") << ",";
    o << "\"activePlaylistId\":\"" << jsonEsc(s.activePlaylistId) << "\",";
    o << "\"currentPlaylistIndex\":" << s.currentPlaylistIndex << ",";

    // Modes catalog
    o << "\"modes\":[";
    for (int i = 0; i < modes::kModeCount; ++i) {
        const auto& mi = modes::getModeInfo(static_cast<modes::ModeId>(i));
        if (i > 0) o << ",";
        o << "{\"key\":\"" << mi.key << "\",\"label\":\"" << mi.label
          << "\",\"category\":\"" << mi.category << "\"}";
    }
    o << "],";

    // Mode settings
    const auto& ms = s.modeSettings;
    o << "\"modeSettings\":{";
    o << "\"clock24h\":" << (ms.clock24h ? "true" : "false") << ",";
    o << "\"clockShowSecondsBar\":" << (ms.clockShowSecondsBar ? "true" : "false") << ",";
    o << "\"clockColorStyle\":\"" << jsonEsc(ms.clockColorStyle) << "\",";
    o << "\"weatherShowTemp\":" << (ms.weatherShowTemp ? "true" : "false") << ",";
    o << "\"moonShowPercent\":" << (ms.moonShowPercent ? "true" : "false") << ",";
    o << "\"musicStyle\":\"" << jsonEsc(ms.musicStyle) << "\",";
    o << "\"musicSensitivity\":" << ms.musicSensitivity << ",";
    o << "\"seasonalTheme\":\"" << jsonEsc(ms.seasonalTheme) << "\",";
    o << "\"sunGradualEffect\":" << (ms.sunGradualEffect ? "true" : "false") << ",";
    o << "\"sunGradualWindowMin\":" << ms.sunGradualWindowMin << ",";
    o << "\"printerAlertFlash\":" << (ms.printerAlertFlash ? "true" : "false") << ",";
    o << "\"pixelArtScene\":\"" << jsonEsc(ms.pixelArtScene) << "\",";
    o << "\"motionStyle\":\"" << jsonEsc(ms.motionStyle) << "\",";
    o << "\"mirrorFlipX\":" << (ms.mirrorFlipX ? "true" : "false") << ",";
    o << "\"mirrorPalette\":\"" << jsonEsc(ms.mirrorPalette) << "\",";
    o << "\"heartRateShowEcg\":" << (ms.heartRateShowEcg ? "true" : "false") << ",";
    o << "\"airQualityShowCo2\":" << (ms.airQualityShowCo2 ? "true" : "false");
    o << "},";

    // Playlists
    o << "\"playlists\":[";
    for (size_t i = 0; i < s.playlists.size(); ++i) {
        if (i > 0) o << ",";
        const auto& pl = s.playlists[i];
        o << "{\"id\":\"" << jsonEsc(pl.id) << "\",\"name\":\"" << jsonEsc(pl.name) << "\",\"pages\":[";
        for (size_t j = 0; j < pl.pages.size(); ++j) {
            if (j > 0) o << ",";
            const auto& pg = pl.pages[j];
            o << "{\"mode\":\"" << modes::getModeInfo(pg.mode).key
              << "\",\"durationSec\":" << pg.durationSec
              << ",\"mediaId\":\"" << jsonEsc(pg.mediaId)
              << "\",\"sceneVariant\":\"" << jsonEsc(pg.sceneVariant) << "\"}";
        }
        o << "]}";
    }
    o << "],";

    // Night schedule
    const auto& ns = s.nightSchedule;
    o << "\"nightSchedule\":{";
    o << "\"dimEnabled\":" << (ns.dimEnabled ? "true" : "false") << ",";
    o << "\"dimStartHour\":" << static_cast<int>(ns.dimStartHour) << ",";
    o << "\"dimStartMinute\":" << static_cast<int>(ns.dimStartMinute) << ",";
    o << "\"dimEndHour\":" << static_cast<int>(ns.dimEndHour) << ",";
    o << "\"dimEndMinute\":" << static_cast<int>(ns.dimEndMinute) << ",";
    o << "\"dimBrightness\":" << static_cast<int>(ns.dimBrightness) << ",";
    o << "\"autoOffEnabled\":" << (ns.autoOffEnabled ? "true" : "false") << ",";
    o << "\"offStartHour\":" << static_cast<int>(ns.offStartHour) << ",";
    o << "\"offStartMinute\":" << static_cast<int>(ns.offStartMinute) << ",";
    o << "\"offEndHour\":" << static_cast<int>(ns.offEndHour) << ",";
    o << "\"offEndMinute\":" << static_cast<int>(ns.offEndMinute) << ",";
    o << "\"wakeOnMotion\":" << (ns.wakeOnMotion ? "true" : "false") << ",";
    o << "\"wakeDurationSec\":" << ns.wakeDurationSec;
    o << "},";

    // Schedules
    o << "\"schedules\":[";
    for (size_t i = 0; i < s.schedules.size(); ++i) {
        if (i > 0) o << ",";
        const auto& r = s.schedules[i];
        o << "{\"enabled\":" << (r.enabled ? "true" : "false")
          << ",\"hour\":" << static_cast<int>(r.hour)
          << ",\"minute\":" << static_cast<int>(r.minute)
          << ",\"daysMask\":" << static_cast<int>(r.daysMask)
          << ",\"action\":\"" << jsonEsc(r.action)
          << "\",\"value\":\"" << jsonEsc(r.value)
          << "\",\"brightness\":" << static_cast<int>(r.brightness) << "}";
    }
    o << "],";

    // Network, Location, Sensors
    o << "\"network\":{\"wifiSsid\":\"" << jsonEsc(s.network.wifiSsid)
      << "\",\"hostname\":\"" << jsonEsc(s.network.hostname)
      << "\",\"haEnabled\":" << (s.network.haEnabled ? "true" : "false")
      << ",\"haUrl\":\"" << jsonEsc(s.network.haUrl)
      << "\",\"mqttHost\":\"" << jsonEsc(s.network.mqttHost)
      << "\",\"mqttPort\":" << s.network.mqttPort
      << ",\"haWeatherEntity\":\"" << jsonEsc(s.network.haWeatherEntity)
      << "\",\"haMediaEntity\":\"" << jsonEsc(s.network.haMediaEntity)
      << "\",\"haPrinterProgressEntity\":\"" << jsonEsc(s.network.haPrinterProgressEntity)
      << "\",\"haPrinterStateEntity\":\"" << jsonEsc(s.network.haPrinterStateEntity)
      << "\",\"haTempEntity\":\"" << jsonEsc(s.network.haTempEntity)
      << "\",\"haAqiEntity\":\"" << jsonEsc(s.network.haAqiEntity) << "\"},";

    o << "\"location\":{\"zipCode\":\"" << jsonEsc(s.location.zipCode)
      << "\",\"locationLabel\":\"" << jsonEsc(s.location.locationLabel)
      << "\",\"latitude\":" << s.location.latitude
      << ",\"longitude\":" << s.location.longitude
      << ",\"tzOffsetMinutes\":" << s.location.tzOffsetMinutes
      << ",\"useFahrenheit\":" << (s.location.useFahrenheit ? "true" : "false") << "},";

    o << "\"sensorHw\":{\"audioSource\":\"" << jsonEsc(s.sensorHw.audioSource)
      << "\",\"mmwaveModel\":\"" << jsonEsc(s.sensorHw.mmwaveModel)
      << "\",\"envSensorModel\":\"" << jsonEsc(s.sensorHw.envSensorModel)
      << "\",\"cameraUrl\":\"" << jsonEsc(s.sensorHw.cameraUrl) << "\"},";

    // Media list
    auto mList = media.list();
    o << "\"media\":[";
    for (size_t i = 0; i < mList.size(); ++i) {
        if (i > 0) o << ",";
        o << "{\"id\":\"" << jsonEsc(mList[i]->id)
          << "\",\"name\":\"" << jsonEsc(mList[i]->name)
          << "\",\"animated\":" << (mList[i]->animated ? "true" : "false")
          << ",\"frames\":" << mList[i]->frames.size() << "}";
    }
    o << "],";

    // Live telemetry
    o << "\"live\":{";
    o << "\"wifiConnected\":true,\"haConnected\":" << (s.network.haEnabled ? "true" : "false")
      << ",\"mqttConnected\":" << (s.network.mqttEnabled ? "true" : "false")
      << ",\"weatherTempC\":" << s.live.weatherTempC
      << ",\"weatherCondition\":" << static_cast<int>(s.live.weatherCondition)
      << ",\"printerProgress\":" << s.live.printerProgress
      << ",\"printerState\":" << static_cast<int>(s.live.printerState)
      << ",\"heartRateBpm\":" << s.live.heartRateBpm
      << ",\"indoorTempC\":" << s.live.indoorTempC
      << ",\"indoorHumidity\":" << s.live.indoorHumidity
      << ",\"aqi\":" << s.live.aqi
      << ",\"co2Ppm\":" << s.live.co2Ppm
      << ",\"motionDetected\":" << (s.live.motionDetected ? "true" : "false");
    o << "}}";
    return o.str();
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    core::AppState state;
    core::MediaStore media;
    core::DisplayEngine engine;

    auto t0 = std::chrono::steady_clock::now();
    updateSystemClock(state);
    engine.setMode(state.activeMode, state, false);

    std::string line;
    uint8_t rgb[kMatrixPixels * 3];

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        auto now = std::chrono::steady_clock::now();
        uint32_t nowMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count()
        );

        if (cmd == "TICK") {
            float dt = 0.04f;
            iss >> dt;
            updateSystemClock(state);
            // Auto-decay synthetic motion after 2s
            if (state.live.motionDetected && (nowMs - state.live.lastMotionMs) > 2000u) {
                state.live.motionDetected = false;
                state.live.motionIntensity = 0.0f;
            }
            if (state.live.audioActive && (nowMs - state.live.lastAudioMs) > 1500u) {
                state.live.audioActive = false;
            }
            engine.tick(dt, nowMs, state, media);
            gfx::Canvas framed = engine.output();
            framed.scaleBrightness(state.effectiveBrightness);
            framed.copyToRGB888(rgb);
            std::cout << "FRAME " << bytesToHex(rgb, sizeof(rgb)) << "\n" << std::flush;
        } else if (cmd == "STATE") {
            updateSystemClock(state);
            std::cout << "STATE " << serializeStateJson(state, media) << "\n" << std::flush;
        } else if (cmd == "SET") {
            std::string key, val;
            iss >> key;
            std::getline(iss, val);
            if (!val.empty() && val[0] == ' ') val.erase(0, 1);

            if (key == "powerOn") state.powerOn = (val == "1" || val == "true");
            else if (key == "brightness") state.brightness = static_cast<uint8_t>(std::clamp(std::atoi(val.c_str()), 0, 255));
            else if (key == "animationSpeed") state.animationSpeed = std::clamp(static_cast<float>(std::atof(val.c_str())), 0.25f, 3.0f);
            else if (key == "primaryColor") state.primaryColor = gfx::RGB::fromHex(static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10)));
            else if (key == "accentColor") state.accentColor = gfx::RGB::fromHex(static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10)));
            else if (key == "transition") state.transition = gfx::transitionFromName(val.c_str());
            else if (key == "activeMode") {
                state.playlistPlaying = false;
                engine.setMode(modes::modeFromKey(val.c_str()), state, true);
            } else if (key == "playlistPlaying") {
                state.playlistPlaying = (val == "1" || val == "true");
                engine.scheduler().resetPageTimer();
            } else if (key == "activePlaylistId") {
                state.activePlaylistId = val;
                state.currentPlaylistIndex = 0;
                engine.scheduler().resetPageTimer();
            } else if (key == "playlistNext") {
                engine.nextPlaylistPage(state);
            } else if (key == "playlistPrev") {
                engine.prevPlaylistPage(state);
            }
            // Mode settings
            else if (key == "ms.clock24h") state.modeSettings.clock24h = (val == "1" || val == "true");
            else if (key == "ms.clockShowSecondsBar") state.modeSettings.clockShowSecondsBar = (val == "1" || val == "true");
            else if (key == "ms.clockColorStyle") state.modeSettings.clockColorStyle = val;
            else if (key == "ms.weatherShowTemp") state.modeSettings.weatherShowTemp = (val == "1" || val == "true");
            else if (key == "ms.moonShowPercent") state.modeSettings.moonShowPercent = (val == "1" || val == "true");
            else if (key == "ms.musicStyle") state.modeSettings.musicStyle = val;
            else if (key == "ms.seasonalTheme") state.modeSettings.seasonalTheme = val;
            else if (key == "ms.sunGradualEffect") state.modeSettings.sunGradualEffect = (val == "1" || val == "true");
            else if (key == "ms.printerAlertFlash") state.modeSettings.printerAlertFlash = (val == "1" || val == "true");
            else if (key == "ms.pixelArtScene") state.modeSettings.pixelArtScene = val;
            else if (key == "ms.motionStyle") state.modeSettings.motionStyle = val;
            else if (key == "ms.mirrorFlipX") state.modeSettings.mirrorFlipX = (val == "1" || val == "true");
            else if (key == "ms.mirrorPalette") state.modeSettings.mirrorPalette = val;
            else if (key == "ms.heartRateShowEcg") state.modeSettings.heartRateShowEcg = (val == "1" || val == "true");
            else if (key == "ms.airQualityShowCo2") state.modeSettings.airQualityShowCo2 = (val == "1" || val == "true");
            // Live sensor injection
            else if (key == "live.weatherCondition") state.live.weatherCondition = static_cast<core::WeatherCondition>(std::clamp(std::atoi(val.c_str()), 0, 8));
            else if (key == "live.weatherTempC") state.live.weatherTempC = static_cast<float>(std::atof(val.c_str()));
            else if (key == "live.printerState") state.live.printerState = static_cast<core::PrinterState>(std::clamp(std::atoi(val.c_str()), 0, 5));
            else if (key == "live.printerProgress") state.live.printerProgress = std::clamp(static_cast<float>(std::atof(val.c_str())), 0.0f, 100.0f);
            else if (key == "live.heartRateBpm") { state.live.heartRateBpm = static_cast<float>(std::atof(val.c_str())); state.live.vitalPresent = true; state.live.vitalValid = true; }
            else if (key == "live.aqi") state.live.aqi = std::clamp(std::atoi(val.c_str()), 0, 500);
            else if (key == "live.indoorTempC") state.live.indoorTempC = static_cast<float>(std::atof(val.c_str()));
            else if (key == "live.motion") {
                float mx = 0.5f, my = 0.5f;
                std::istringstream ms2(val);
                ms2 >> mx >> my;
                state.live.motionX = mx;
                state.live.motionY = my;
                state.live.motionDetected = true;
                state.live.motionIntensity = 1.0f;
                state.live.lastMotionMs = nowMs;
            }
            // Location & config
            else if (key == "loc.zipCode") {
                state.location.zipCode = val;
                double lat = 0, lon = 0; int tz = state.location.tzOffsetMinutes; std::string lbl;
                if (core::lookupUsZipOffline(val, lat, lon, tz, lbl)) {
                    state.location.latitude = lat;
                    state.location.longitude = lon;
                    state.location.locationLabel = lbl;
                }
            }
            else if (key == "loc.latitude") state.location.latitude = std::atof(val.c_str());
            else if (key == "loc.longitude") state.location.longitude = std::atof(val.c_str());
            else if (key == "loc.tzOffsetMinutes") state.location.tzOffsetMinutes = std::atoi(val.c_str());
            else if (key == "loc.useFahrenheit") state.location.useFahrenheit = (val == "1" || val == "true");
            else if (key == "net.wifiSsid") state.network.wifiSsid = val;
            else if (key == "net.hostname") state.network.hostname = val;
            else if (key == "net.haEnabled") { state.network.haEnabled = (val == "1" || val == "true"); state.network.mqttEnabled = state.network.haEnabled; }
            else if (key == "net.haUrl") state.network.haUrl = val;
            else if (key == "net.mqttHost") state.network.mqttHost = val;
            else if (key == "net.mqttPort") state.network.mqttPort = static_cast<uint16_t>(std::atoi(val.c_str()));
            else if (key == "hw.audioSource") state.sensorHw.audioSource = val;
            else if (key == "hw.mmwaveModel") state.sensorHw.mmwaveModel = val;
            else if (key == "hw.envSensorModel") state.sensorHw.envSensorModel = val;
            else if (key == "hw.cameraUrl") state.sensorHw.cameraUrl = val;
            // Night schedule
            else if (key == "ns.dimEnabled") state.nightSchedule.dimEnabled = (val == "1" || val == "true");
            else if (key == "ns.dimStartHour") state.nightSchedule.dimStartHour = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.dimStartMinute") state.nightSchedule.dimStartMinute = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.dimEndHour") state.nightSchedule.dimEndHour = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.dimEndMinute") state.nightSchedule.dimEndMinute = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.dimBrightness") state.nightSchedule.dimBrightness = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.autoOffEnabled") state.nightSchedule.autoOffEnabled = (val == "1" || val == "true");
            else if (key == "ns.offStartHour") state.nightSchedule.offStartHour = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.offStartMinute") state.nightSchedule.offStartMinute = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.offEndHour") state.nightSchedule.offEndHour = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.offEndMinute") state.nightSchedule.offEndMinute = static_cast<uint8_t>(std::atoi(val.c_str()));
            else if (key == "ns.wakeOnMotion") state.nightSchedule.wakeOnMotion = (val == "1" || val == "true");

            std::cout << "OK\n" << std::flush;
        } else if (cmd == "MIRROR_HEX") {
            std::string hex;
            iss >> hex;
            auto bytes = hexToBytes(hex);
            if (bytes.size() >= kMatrixPixels * 3) {
                std::memcpy(state.live.mirrorRgb.data(), bytes.data(), kMatrixPixels * 3);
                state.live.mirrorFrameValid = true;
                state.live.lastMirrorMs = nowMs;
            }
        } else if (cmd == "AUDIO_HEX") {
            std::string hex;
            iss >> hex;
            auto bytes = hexToBytes(hex);
            if (bytes.size() >= 16) {
                float sum = 0.0f;
                for (int i = 0; i < 16; ++i) {
                    state.live.audioBands[i] = static_cast<float>(bytes[i]) / 255.0f;
                    sum += state.live.audioBands[i];
                }
                state.live.audioLevel = sum / 16.0f;
                state.live.audioActive = true;
                state.live.lastAudioMs = nowMs;
            }
        } else if (cmd == "ALBUM_HEX") {
            std::string hex;
            iss >> hex;
            auto bytes = hexToBytes(hex);
            if (bytes.size() >= kMatrixPixels * 3) {
                std::memcpy(state.live.albumArtRgb.data(), bytes.data(), kMatrixPixels * 3);
                state.live.albumArtValid = true;
                state.live.mediaPlaying = true;
            }
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "UPLOAD_MPX") {
            std::string id, name, hex;
            iss >> id >> name >> hex;
            std::replace(name.begin(), name.end(), '_', ' ');
            auto bytes = hexToBytes(hex);
            bool ok = media.putMpx(id, name, bytes.data(), bytes.size());
            if (ok) {
                state.modeSettings.pixelArtScene = id;
                engine.setMode(modes::ModeId::PixelArt, state, true);
            }
            std::cout << (ok ? "OK\n" : "ERR\n") << std::flush;
        } else if (cmd == "DEL_MEDIA") {
            std::string id;
            iss >> id;
            media.remove(id);
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "CLEAR_PLAYLISTS") {
            state.playlists.clear();
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "ADD_PLAYLIST") {
            std::string id, name;
            iss >> id;
            std::getline(iss, name);
            if (!name.empty() && name[0] == ' ') name.erase(0, 1);
            core::Playlist pl;
            pl.id = id;
            pl.name = name.empty() ? id : name;
            state.playlists.push_back(pl);
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "ADD_PAGE") {
            std::string plId, modeKey;
            int dur = 15;
            iss >> plId >> modeKey >> dur;
            if (auto* pl = state.findPlaylistMut(plId)) {
                pl->pages.push_back({modes::modeFromKey(modeKey.c_str()), static_cast<uint16_t>(std::max(3, dur)), "", ""});
            }
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "CLEAR_SCHEDULES") {
            state.schedules.clear();
            std::cout << "OK\n" << std::flush;
        } else if (cmd == "ADD_SCHEDULE") {
            core::ScheduleRule r;
            int en = 1, h = 7, m = 0, dm = 0x7F, br = 180;
            iss >> en >> h >> m >> dm >> r.action >> r.value >> br;
            r.enabled = (en != 0);
            r.hour = static_cast<uint8_t>(h);
            r.minute = static_cast<uint8_t>(m);
            r.daysMask = static_cast<uint8_t>(dm);
            r.brightness = static_cast<uint8_t>(br);
            state.schedules.push_back(r);
            std::cout << "OK\n" << std::flush;
        }
    }
    return 0;
}
