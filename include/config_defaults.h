#pragma once

#include <cstdint>

#ifndef MATRIX_WIDTH
#define MATRIX_WIDTH 16
#endif

#ifndef MATRIX_HEIGHT
#define MATRIX_HEIGHT 16
#endif

#ifndef FW_VERSION
#define FW_VERSION "1.0.0"
#endif

static constexpr uint8_t kMatrixWidth = MATRIX_WIDTH;
static constexpr uint8_t kMatrixHeight = MATRIX_HEIGHT;
static constexpr uint16_t kMatrixPixels = kMatrixWidth * kMatrixHeight;

// Target frame rate for smooth animations behind acrylic diffuser
static constexpr uint16_t kTargetFps = 30;
static constexpr uint32_t kFrameIntervalMs = 1000 / kTargetFps;

// Hardware pin defaults (overridable via config.json / Web UI)
#ifndef DEFAULT_LED_PIN
#define DEFAULT_LED_PIN 4
#endif
#ifndef DEFAULT_I2S_SCK_PIN
#define DEFAULT_I2S_SCK_PIN 5
#endif
#ifndef DEFAULT_I2S_WS_PIN
#define DEFAULT_I2S_WS_PIN 6
#endif
#ifndef DEFAULT_I2S_SD_PIN
#define DEFAULT_I2S_SD_PIN 7
#endif
#ifndef DEFAULT_MMWAVE_RX_PIN
#define DEFAULT_MMWAVE_RX_PIN 16
#endif
#ifndef DEFAULT_MMWAVE_TX_PIN
#define DEFAULT_MMWAVE_TX_PIN 17
#endif
#ifndef DEFAULT_I2C_SDA_PIN
#define DEFAULT_I2C_SDA_PIN 8
#endif
#ifndef DEFAULT_I2C_SCL_PIN
#define DEFAULT_I2C_SCL_PIN 9
#endif
#ifndef DEFAULT_PIR_PIN
#define DEFAULT_PIR_PIN 10
#endif

static constexpr uint16_t kDefaultMaxCurrentMa = 2400; // Safe 5V 3A supply budget
static constexpr uint16_t kMaxMediaFrames = 120;       // Max frames per uploaded GIF (.mpx)
static constexpr uint8_t kMaxPlaylists = 8;
static constexpr uint8_t kMaxPlaylistItems = 24;
static constexpr uint8_t kMaxSchedules = 16;

// Default Wi-Fi credentials (falls back to Setup AP if unreachable)
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID "iot"
#endif
#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS "Zeppelin96$"
#endif
