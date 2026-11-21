#ifdef ARDUINO

#include <Arduino.h>

#include "config_defaults.h"
#include "core/app_state.h"
#include "core/display_engine.h"
#include "core/media_store.h"
#include "hw/audio_input.h"
#include "hw/env_sensors.h"
#include "hw/matrix_driver.h"
#include "hw/mmwave_sensor.h"
#include "net/config_store.h"
#include "net/ha_client.h"
#include "net/web_server.h"
#include "net/wifi_manager.h"

namespace {
core::AppState gState;
core::MediaStore gMedia;
core::DisplayEngine gEngine;

hw::MatrixDriver gMatrix;
hw::AudioInput gAudio;
hw::MmWaveSensor gMmWave;
hw::EnvSensors gEnv;

net::ConfigStore gStore;
net::WifiManager gWifi;
net::HomeAssistantClient gHA;
net::MatrixWebServer gWeb;

uint32_t gLastFrameMs = 0;
} // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[MatrixDisplay] Booting Smart 16x16 Acrylic LED Matrix v" FW_VERSION);

    // 1. Mount LittleFS & restore persistent config + uploaded pixel art / GIFs
    if (gStore.begin()) {
        Serial.println("[MatrixDisplay] LittleFS mounted");
        gStore.loadConfig(gState);
        gStore.loadAllMedia(gMedia);
    } else {
        Serial.println("[MatrixDisplay] LittleFS mount failed; continuing with defaults");
    }

    // 2. Start Wi-Fi & Web UI before LED RMT/I2S DMA peripherals so initial NVS/PHY flash
    // calibration completes with no active DMA/RMT interrupts.
    Serial.println("[MatrixDisplay] Starting Wi-Fi, Home Assistant & Web UI...");
    gWifi.begin(gState);
    gHA.begin(gState, gEngine);
    gWeb.begin(gState, gEngine, gMedia, gStore, gWifi);

    // 3. Initialize hardware peripherals (works immediately offline!)
    Serial.println("[MatrixDisplay] Init LED matrix & sensors...");
    gMatrix.begin(gState.matrixHw);
    gAudio.begin(gState.sensorHw);
    gMmWave.begin(gState.sensorHw);
    gEnv.begin(gState.sensorHw);

    gEngine.setMode(gState.activeMode, gState, false);
    gLastFrameMs = millis();
    Serial.printf("[MatrixDisplay] Ready! Open http://%s in your browser.\n",
                  gState.live.ipAddress.c_str());
}

void loop() {
    uint32_t nowMs = millis();

    // Poll sensors, Wi-Fi, Home Assistant, and Web UI non-blockingly
    gAudio.poll(nowMs, gState);
    gMmWave.poll(nowMs, gState);
    gEnv.poll(nowMs, gState);
    gWifi.poll(nowMs, gState);
    gHA.poll(nowMs, gState, gEngine);
    gWeb.poll(nowMs, gEngine);

    // Render smooth 30 FPS frame to 16x16 acrylic LED matrix
    uint32_t elapsed = nowMs - gLastFrameMs;
    if (elapsed >= kFrameIntervalMs) {
        gLastFrameMs = nowMs;
        float dtSec = static_cast<float>(elapsed) / 1000.0f;
        gEngine.tick(dtSec, nowMs, gState, gMedia);
        gMatrix.applyConfig(gState.matrixHw);
        gMatrix.show(gEngine.output(), gState.effectiveBrightness, gState.effectivePowerOn);
    }
}

#endif // ARDUINO
