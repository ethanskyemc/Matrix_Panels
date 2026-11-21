#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "config_defaults.h"
#include "gfx/transitions.h"
#include "gfx/types.h"
#include "modes/mode_base.h"

namespace core {

struct LocalTime {
    int year = 2026;
    int month = 11;    // 1..12
    int day = 18;      // 1..31
    int hour = 17;     // 0..23
    int minute = 7;    // 0..59
    int second = 0;    // 0..59
    int weekday = 3;   // 0=Sun..6=Sat
    int64_t unixEpoch = 1795050420;
    bool synced = false;
};

enum class WeatherCondition : uint8_t {
    ClearDay = 0,
    ClearNight,
    PartlyCloudy,
    Cloudy,
    Rain,
    Storm,
    Snow,
    Fog,
    Windy
};

enum class PrinterState : uint8_t {
    Idle = 0,
    Heating,
    Printing,
    Paused,
    Complete,
    Error
};

struct PlaylistPage {
    modes::ModeId mode = modes::ModeId::Clock;
    uint16_t durationSec = 20;
    std::string mediaId;       // Used when mode == PixelArt (empty = built-in scene)
    std::string sceneVariant;  // Optional per-page variant override
};

struct Playlist {
    std::string id = "default";
    std::string name = "Daily Mix";
    std::vector<PlaylistPage> pages;
};

struct ScheduleRule {
    bool enabled = true;
    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t daysMask = 0x7F; // Bit 0 = Sun .. Bit 6 = Sat
    std::string action = "mode"; // "power_on", "power_off", "mode", "playlist", "brightness"
    std::string value = "sunrise_sunset";
    uint8_t brightness = 180;
};

struct NightSchedule {
    bool dimEnabled = true;
    uint8_t dimStartHour = 22;
    uint8_t dimStartMinute = 0;
    uint8_t dimEndHour = 7;
    uint8_t dimEndMinute = 0;
    uint8_t dimBrightness = 40; // 0..255

    bool autoOffEnabled = false;
    uint8_t offStartHour = 23;
    uint8_t offStartMinute = 30;
    uint8_t offEndHour = 6;
    uint8_t offEndMinute = 30;
    bool wakeOnMotion = true;
    uint16_t wakeDurationSec = 60;
};

struct MatrixHardwareConfig {
    uint8_t ledPin = DEFAULT_LED_PIN;
    bool serpentine = true;
    uint8_t rotation = 0; // 0, 1(90), 2(180), 3(270)
    bool flipX = true;
    bool flipY = false;
    uint16_t maxCurrentMa = kDefaultMaxCurrentMa;
    bool gammaCorrection = true;
};

struct SensorHardwareConfig {
    bool audioEnabled = true;
    std::string audioSource = "i2s"; // "i2s", "adc", "stream"
    uint8_t i2sSckPin = DEFAULT_I2S_SCK_PIN;
    uint8_t i2sWsPin = DEFAULT_I2S_WS_PIN;
    uint8_t i2sSdPin = DEFAULT_I2S_SD_PIN;
    float micGain = 1.2f;

    bool mmwaveEnabled = true;
    std::string mmwaveModel = "mr60bha1"; // "mr60bha1", "mr60bha2", "ld2410", "ha"
    uint8_t mmwaveRxPin = DEFAULT_MMWAVE_RX_PIN;
    uint8_t mmwaveTxPin = DEFAULT_MMWAVE_TX_PIN;

    bool envSensorEnabled = true;
    std::string envSensorModel = "bme680"; // "bme680", "sht4x", "scd4x", "ha"
    uint8_t i2cSdaPin = DEFAULT_I2C_SDA_PIN;
    uint8_t i2cSclPin = DEFAULT_I2C_SCL_PIN;

    bool pirEnabled = true;
    uint8_t pirPin = DEFAULT_PIR_PIN;

    std::string cameraUrl = ""; // Optional ESP32-CAM snapshot URL (blank = browser mirror stream)
};

struct NetworkConfig {
    std::string wifiSsid = DEFAULT_WIFI_SSID;
    std::string wifiPassword = DEFAULT_WIFI_PASS;
    std::string hostname = "matrix-acrylic";

    // Home Assistant MQTT + REST integration
    bool haEnabled = false;
    std::string haUrl = "http://homeassistant.local:8123";
    std::string haToken = "";
    bool mqttEnabled = false;
    std::string mqttHost = "homeassistant.local";
    uint16_t mqttPort = 1883;
    std::string mqttUser = "";
    std::string mqttPassword = "";
    std::string mqttPrefix = "matrix_acrylic";

    // Mapped HA entities
    std::string haWeatherEntity = "weather.home";
    std::string haMediaEntity = "media_player.living_room";
    std::string haPrinterProgressEntity = "sensor.3d_printer_progress";
    std::string haPrinterStateEntity = "sensor.3d_printer_state";
    std::string haTempEntity = "sensor.living_room_temperature";
    std::string haHumidityEntity = "sensor.living_room_humidity";
    std::string haAqiEntity = "sensor.living_room_aqi";
    std::string haHeartRateEntity = "sensor.mmwave_heart_rate";
    std::string haMotionEntity = "binary_sensor.living_room_presence";
};

struct LocationConfig {
    std::string zipCode = "94103";
    std::string locationLabel = "San Francisco, CA";
    double latitude = 37.7749;
    double longitude = -122.4194;
    std::string timezone = "America/Los_Angeles";
    int tzOffsetMinutes = -480; // UTC-8 default
    bool useFahrenheit = true;
};

struct ModeSettings {
    // Clock
    bool clock24h = false;
    bool clockShowSecondsBar = true;
    std::string clockColorStyle = "gradient"; // "solid", "gradient", "rainbow", "time_of_day"

    // Weather
    bool weatherShowTemp = true;

    // Moon
    bool moonShowPercent = false;

    // Music visualizer
    std::string musicStyle = "spectrum"; // "spectrum", "mirror", "radial", "vu"
    float musicSensitivity = 1.0f;

    // Seasonal
    std::string seasonalTheme = "auto"; // "auto","spring","summer","autumn","winter","halloween","christmas","newyear","valentine"

    // Sunrise/Sunset
    bool sunGradualEffect = true;
    uint16_t sunGradualWindowMin = 30;

    // 3D Printer
    bool printerAlertFlash = true;

    // Pixel art
    std::string pixelArtScene = "cabin"; // built-in name or uploaded mediaId

    // Motion reactive
    std::string motionStyle = "ripples"; // "ripples", "swarm", "aura"

    // Mirror
    bool mirrorFlipX = true;
    std::string mirrorPalette = "truecolor"; // "truecolor", "thermal", "retro", "neon"

    // Heart rate
    bool heartRateShowEcg = true;

    // Air quality
    bool airQualityShowCo2 = true;
};

struct LiveData {
    // Network status
    bool wifiConnected = false;
    bool apModeActive = false;
    bool haConnected = false;
    bool mqttConnected = false;
    int rssi = -60;
    std::string ipAddress = "192.168.4.1";

    // Time
    LocalTime time;

    // Weather
    WeatherCondition weatherCondition = WeatherCondition::PartlyCloudy;
    float weatherTempC = 21.5f;
    int weatherHumidity = 52;
    std::string weatherSummary = "Partly Cloudy";
    bool weatherValid = true;

    // Audio spectrum (16 bands normalized 0..1) + overall energy
    std::array<float, 16> audioBands{};
    float audioLevel = 0.0f;
    bool audioActive = false;
    uint32_t lastAudioMs = 0;

    // Album art (16x16 RGB888)
    std::array<uint8_t, kMatrixPixels * 3> albumArtRgb{};
    bool albumArtValid = false;
    bool mediaPlaying = false;
    std::string mediaTitle = "";
    std::string mediaArtist = "";

    // 3D Printer
    PrinterState printerState = PrinterState::Printing;
    float printerProgress = 68.0f; // 0..100
    float nozzleTempC = 215.0f;
    float bedTempC = 60.0f;
    uint32_t printerEtaMinutes = 42;
    bool printerCompletionAck = false;

    // Motion / Presence
    bool motionDetected = false;
    float motionIntensity = 0.0f; // 0..1
    float motionX = 0.5f;         // 0..1
    float motionY = 0.5f;         // 0..1
    uint32_t lastMotionMs = 0;

    // Mirror camera live 16x16 RGB888 frame
    std::array<uint8_t, kMatrixPixels * 3> mirrorRgb{};
    bool mirrorFrameValid = false;
    uint32_t lastMirrorMs = 0;

    // Heart Rate (mmWave vital signs)
    float heartRateBpm = 72.0f;
    float breathRateRpm = 15.0f;
    bool vitalPresent = true;
    bool vitalValid = true;

    // Air quality & environment
    float indoorTempC = 22.4f;
    float indoorHumidity = 46.0f;
    int aqi = 28;        // 0..500 US AQI
    int co2Ppm = 540;    // ppm
    int vocIndex = 85;
    bool envValid = true;
};

struct AppState {
    // Global controls
    bool powerOn = true;
    uint8_t brightness = 190; // 0..255
    float animationSpeed = 1.0f; // 0.25..3.0
    gfx::RGB primaryColor = gfx::RGB(64, 196, 255);
    gfx::RGB accentColor = gfx::RGB(255, 104, 180);
    gfx::TransitionType transition = gfx::TransitionType::Crossfade;
    uint16_t transitionMs = 650;

    // Active display mode & playlist state
    modes::ModeId activeMode = modes::ModeId::Clock;
    bool playlistPlaying = false;
    std::string activePlaylistId = "default";
    size_t currentPlaylistIndex = 0;

    // Collections
    std::vector<Playlist> playlists;
    std::vector<ScheduleRule> schedules;
    NightSchedule nightSchedule;
    MatrixHardwareConfig matrixHw;
    SensorHardwareConfig sensorHw;
    NetworkConfig network;
    LocationConfig location;
    ModeSettings modeSettings;

    // Live runtime telemetry & sensor inputs
    LiveData live;

    // Derived effective runtime outputs (computed by Scheduler)
    bool effectivePowerOn = true;
    uint8_t effectiveBrightness = 190;
    bool nightDimActive = false;
    bool autoOffActive = false;

    AppState() {
        initDefaults();
    }

    void initDefaults() {
        playlists.clear();
        Playlist daily;
        daily.id = "default";
        daily.name = "Daily Mix";
        daily.pages = {
            {modes::ModeId::Clock, 15, "", ""},
            {modes::ModeId::Weather, 15, "", ""},
            {modes::ModeId::MoonPhase, 12, "", ""},
            {modes::ModeId::Fireplace, 15, "", ""},
            {modes::ModeId::SpinningGlobe, 15, "", ""}
        };
        playlists.push_back(daily);

        Playlist ambient;
        ambient.id = "ambient";
        ambient.name = "Cozy Ambient";
        ambient.pages = {
            {modes::ModeId::Fireplace, 25, "", ""},
            {modes::ModeId::Waves, 25, "", ""},
            {modes::ModeId::Aquarium, 25, "", ""},
            {modes::ModeId::SolarSystem, 25, "", ""}
        };
        playlists.push_back(ambient);

        Playlist desk;
        desk.id = "desk";
        desk.name = "Smart Desk";
        desk.pages = {
            {modes::ModeId::Clock, 15, "", ""},
            {modes::ModeId::AirQuality, 12, "", ""},
            {modes::ModeId::Printer3D, 15, "", ""},
            {modes::ModeId::MusicVisualizer, 15, "", ""},
            {modes::ModeId::HeartRate, 12, "", ""}
        };
        playlists.push_back(desk);

        schedules.clear();
        ScheduleRule morning;
        morning.enabled = true;
        morning.hour = 7;
        morning.minute = 0;
        morning.daysMask = 0x7F;
        morning.action = "mode";
        morning.value = "sunrise_sunset";
        morning.brightness = 180;
        schedules.push_back(morning);

        ScheduleRule evening;
        evening.enabled = true;
        evening.hour = 20;
        evening.minute = 0;
        evening.daysMask = 0x7F;
        evening.action = "playlist";
        evening.value = "ambient";
        evening.brightness = 140;
        schedules.push_back(evening);
    }

    const Playlist* findPlaylist(const std::string& id) const {
        for (const auto& p : playlists) {
            if (p.id == id) return &p;
        }
        return playlists.empty() ? nullptr : &playlists.front();
    }

    Playlist* findPlaylistMut(const std::string& id) {
        for (auto& p : playlists) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
};

} // namespace core
