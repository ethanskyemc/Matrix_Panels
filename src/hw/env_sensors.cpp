#include "hw/env_sensors.h"

#include <algorithm>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_SHT4x.h>
#include <SensirionI2CScd4x.h>
#endif

namespace hw {

#ifdef ARDUINO
static Adafruit_BME680 gBme;
static Adafruit_SHT4x gSht4;
static SensirionI2CScd4x gScd4x;
#endif

void EnvSensors::begin(const core::SensorHardwareConfig& cfg) {
    bmeOk_ = shtOk_ = scdOk_ = false;
#ifdef ARDUINO
    if (cfg.pirEnabled) {
        pinMode(cfg.pirPin, INPUT);
    }
    if (!cfg.envSensorEnabled || cfg.envSensorModel == "ha") return;

    Wire.begin(cfg.i2cSdaPin, cfg.i2cSclPin);
    Wire.setTimeOut(50);
    if (cfg.envSensorModel == "bme680") {
        if (gBme.begin(0x76) || gBme.begin(0x77)) {
            gBme.setTemperatureOversampling(BME680_OS_4X);
            gBme.setHumidityOversampling(BME680_OS_2X);
            gBme.setGasHeater(320, 150);
            bmeOk_ = true;
        }
    } else if (cfg.envSensorModel == "sht4x") {
        if (gSht4.begin()) {
            gSht4.setPrecision(SHT4X_MED_PRECISION);
            shtOk_ = true;
        }
    } else if (cfg.envSensorModel == "scd4x") {
        gScd4x.begin(Wire);
        gScd4x.stopPeriodicMeasurement();
        if (gScd4x.startPeriodicMeasurement() == 0) {
            scdOk_ = true;
        }
    }
#else
    (void)cfg;
#endif
}

void EnvSensors::poll(uint32_t nowMs, core::AppState& state) {
#ifdef ARDUINO
    // PIR Motion sensor check
    if (state.sensorHw.pirEnabled) {
        if (digitalRead(state.sensorHw.pirPin) == HIGH) {
            state.live.motionDetected = true;
            state.live.motionIntensity = 1.0f;
            state.live.lastMotionMs = nowMs;
        } else if ((nowMs - state.live.lastMotionMs) > 4000u) {
            state.live.motionDetected = false;
            state.live.motionIntensity = 0.0f;
        }
    }

    if (nowMs - lastEnvPollMs_ < 5000u) return;
    lastEnvPollMs_ = nowMs;

    if (bmeOk_ && gBme.performReading()) {
        state.live.indoorTempC = gBme.temperature;
        state.live.indoorHumidity = gBme.humidity;
        // Convert gas resistance (Ohms) to approximate 0..300 AQI estimate
        float gasKOhm = static_cast<float>(gBme.gas_resistance) / 1000.0f;
        int aqi = static_cast<int>(std::clamp(220.0f - gasKOhm * 2.0f, 10.0f, 300.0f));
        state.live.aqi = aqi;
        state.live.envValid = true;
    } else if (shtOk_) {
        sensors_event_t hum, temp;
        if (gSht4.getEvent(&hum, &temp)) {
            state.live.indoorTempC = temp.temperature;
            state.live.indoorHumidity = hum.relative_humidity;
            state.live.envValid = true;
        }
    } else if (scdOk_) {
        uint16_t co2 = 0;
        float t = 0, h = 0;
        if (gScd4x.readMeasurement(co2, t, h) == 0 && co2 > 0) {
            state.live.co2Ppm = co2;
            state.live.indoorTempC = t;
            state.live.indoorHumidity = h;
            state.live.aqi = std::clamp((static_cast<int>(co2) - 400) / 8, 10, 300);
            state.live.envValid = true;
        }
    }
#else
    (void)nowMs; (void)state;
#endif
}

} // namespace hw
