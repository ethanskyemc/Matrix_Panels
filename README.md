# Smart 16×16 RGB LED Acrylic Display

A complete ESP32 / ESP32-S3 firmware, offline-first phone-friendly PWA web interface, Home Assistant integration, and local desktop simulator for a **16×16 RGB LED matrix behind an 8-inch-square frosted acrylic panel**.

---

## Highlights & Display Modes

Designed specifically for low-resolution 16×16 acrylic diffusion with bold 3×5 and 4×6 pixel fonts, perceptual gamma correction, smooth 30 FPS animations, and 7 transition effects (`Crossfade`, `Slide Left`, `Slide Up`, `Wipe`, `Dissolve`, `Iris`, `Instant`):

| Category | Modes Included |
| --- | --- |
| **Utility & Time** | **Clock** (12/24h, stacked bold digits, color themes, seconds bar), **Sunrise & Sunset** (offline NOAA local sun math + optional gradual dawn/dusk lighting glow) |
| **Weather & Astronomy** | **Weather** (animated sun, clouds, rain, lightning, snow, fog + °F/°C temp), **Moon Phase** (offline synodic lunar disc with crater shading + twinkling stars) |
| **Ambient Scenes** | **Fireplace** (heat-diffusion flames, logs, rising embers), **Ocean Waves** (layered seafoam waves), **Aquarium** (swaying kelp, bubbles, swimming fish), **Seasonal Themes** (Auto-calendar + Spring, Summer, Autumn, Winter, Halloween, Christmas, New Year, Valentine) |
| **Space** | **Solar System** (glowing Sun + orbiting planets & ringed Saturn), **Spinning Globe** (3D orthographic rotating Earth with continents, ice caps & atmosphere) |
| **Smart Home & Media** | **Album Art** (16×16 artwork + spinning vinyl fallback), **3D Printer Status** (animated gantry, rising printed layers, states, pulsing green completion alert), **Pixel Art Scenes** (6 built-in animated scenes + uploaded PNG/JPG/GIF `.mpx` gallery) |
| **Reactive & Sensors** | **Music Visualizer** (16-band spectrum, mirror, radial, VU — I2S mic or live phone mic), **Motion Reactive** (ripples, swarm, aura — PIR/mmWave/touch), **Mirror Mode** (live 16×16 camera feed with True Color, Thermal, Retro & Neon palettes), **Heart Rate** (Seeed MR60BHA1/MR60BHA2 60GHz mmWave BPM + synced beating heart & ECG), **Air Quality & Temp** (BME680/SCD4x/SHT4x or HA AQI/CO2/Temp/Humidity) |

---

## Physical Acrylic Enclosure Notes (8" × 8")

- **Panel size**: 8" × 8" (203 mm × 203 mm) with a standard 16×16 WS2812B matrix (typically 160 mm × 160 mm or 10 mm pitch; use a 3D-printed 16×16 black baffle grid to isolate each cell cleanly).
- **Acrylic diffuser**: 3 mm smoked black (30–40% light transmission) or frosted matte white cast acrylic placed 8–12 mm in front of the LEDs for crisp square pixel glow with minimal bleed.
- **Power supply**: 5V 3A–4A regulated USB-C / barrel supply (firmware enforces a configurable `2400 mA` FastLED power budget by default).

---

## Default Hardware Wiring (ESP32-S3-DevKitC-1)

All pins are configurable in `include/config_defaults.h` and `platformio.ini` (an `esp32dev` environment is also included):

| Peripheral | Signal | ESP32-S3 Pin | Notes |
| --- | --- | --- | --- |
| **16×16 WS2812B Matrix** | DIN | `GPIO 4` | 330–470Ω series resistor, 1000µF cap across 5V/GND |
| **I2S Mic (INMP441)** | SCK / WS / SD | `GPIO 5 / 6 / 7` | 3.3V powered; L/R tied to GND |
| **mmWave Vital Sensor** | RX / TX | `GPIO 16 / 17` | Seeed MR60BHA1 / MR60BHA2 (115200 baud) or LD2410 |
| **I2C Air/Temp Sensor** | SDA / SCL | `GPIO 8 / 9` | Auto-supports BME680, SCD4x, or SHT4x |
| **PIR Motion Sensor** | OUT | `GPIO 10` | Optional hardware wake & motion reactive input |

---

## Quick Start: Local Simulator (Test Everything Without Hardware!)

You can compile and run the **exact same C++17 display engine** on macOS/Linux alongside the phone-friendly web UI:

```bash
# 1. Run automated C++ unit & visual verification suite
clang++ -std=c++17 -O2 -Wall -Wextra -DMATRIX_NATIVE=1 -Iinclude -Isrc \
  src/gfx/*.cpp src/core/*.cpp src/modes/*.cpp sim/test_suite.cpp -o /tmp/matrix_test && /tmp/matrix_test

# 2. Launch interactive 16x16 Acrylic Simulator + Phone PWA at http://127.0.0.1:8080
node sim/server.mjs
```

Open `http://127.0.0.1:8080` in your browser to preview all 18 modes behind a frosted acrylic diffuser, test live webcam **Mirror Mode**, stream your microphone to **Music Visualizer**, upload & crop **PNG/JPG/animated GIFs**, build looping **Playlists**, configure **Nighttime Dimming & Schedules**, and test **Home Assistant / 3D Printer / Heart Rate / AQI** sensor states.

---

## Flashing to ESP32 / ESP32-S3 with PlatformIO

```bash
# Build & flash firmware (default: esp32-s3-devkitc-1; or pass -e esp32dev)
pio run -t upload

# Upload the phone-friendly PWA web interface to LittleFS
pio run -t uploadfs
```

On first boot with no Wi-Fi configured, the display runs local modes & playlists immediately and hosts a setup Wi-Fi hotspot **`MatrixDisplay-Setup`** (password `matrix1616`). Connect with your phone and open `http://192.168.4.1` to configure Wi-Fi, Home Assistant, location, and sensors. You can also tap **Add to Home Screen** on iOS or Android to install it as a companion PWA app.

---

## Home Assistant Integration

Enable Home Assistant & MQTT in the **Setup** tab:
- **MQTT Auto-Discovery** automatically exposes Power, Brightness, Display Mode (`light` & `select`), Playlist Selection (`select`), Playlist Loop (`switch`), and local sensor readings (Heart Rate BPM, Temperature, AQI).
- **HA REST API Poller** pulls live Weather, Media Player now-playing info, 3D Printer progress/state, and indoor sensors directly from your Home Assistant instance.
- See `docs/home_assistant_example.yaml` for dashboard cards and automations.
- **Offline-Resilient**: If Wi-Fi or Home Assistant goes offline, local animations, playlists, schedules, moon phase, sunrise/sunset, uploaded art/GIFs, and local sensors keep running uninterrupted.
