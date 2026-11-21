#include "hw/matrix_driver.h"

#ifdef ARDUINO
#include <FastLED.h>
#endif

namespace hw {

#ifdef ARDUINO
static CRGB gLeds[kMatrixPixels];
#endif

uint16_t MatrixDriver::mapXY(int x, int y, const core::MatrixHardwareConfig& cfg) {
    int rx = x, ry = y;
    switch (cfg.rotation & 3) {
        case 1: rx = 15 - y; ry = x; break;
        case 2: rx = 15 - x; ry = 15 - y; break;
        case 3: rx = y;      ry = 15 - x; break;
        default: break;
    }
    if (cfg.flipX) rx = 15 - rx;
    if (cfg.flipY) ry = 15 - ry;

    if (cfg.serpentine && (ry & 1)) {
        return static_cast<uint16_t>(ry * 16 + (15 - rx));
    }
    return static_cast<uint16_t>(ry * 16 + rx);
}

void MatrixDriver::begin(const core::MatrixHardwareConfig& cfg) {
    cfg_ = cfg;
#ifdef ARDUINO
    FastLED.addLeds<WS2812B, DEFAULT_LED_PIN, GRB>(gLeds, kMatrixPixels);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, cfg_.maxCurrentMa);
    FastLED.setBrightness(128);
    FastLED.clear(true);
#endif
    initialized_ = true;
}

void MatrixDriver::applyConfig(const core::MatrixHardwareConfig& cfg) {
    bool powerChanged = (cfg.maxCurrentMa != cfg_.maxCurrentMa);
    cfg_ = cfg;
#ifdef ARDUINO
    if (powerChanged) {
        FastLED.setMaxPowerInVoltsAndMilliamps(5, cfg_.maxCurrentMa);
    }
#else
    (void)powerChanged;
#endif
}

void MatrixDriver::show(const gfx::Canvas& canvas, uint8_t brightness, bool powerOn) {
    if (!initialized_) return;
#ifdef ARDUINO
    if (!powerOn || brightness == 0) {
        FastLED.clear(true);
        return;
    }
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            gfx::RGB p = canvas.getPixel(x, y);
            uint16_t idx = mapXY(x, y, cfg_);
            if (cfg_.gammaCorrection) {
                gLeds[idx] = CRGB(gfx::gammaCorrect8(p.r),
                                  gfx::gammaCorrect8(p.g),
                                  gfx::gammaCorrect8(p.b));
            } else {
                gLeds[idx] = CRGB(p.r, p.g, p.b);
            }
        }
    }
    FastLED.setBrightness(brightness);
    FastLED.show();
#else
    (void)canvas; (void)brightness; (void)powerOn;
#endif
}

} // namespace hw
