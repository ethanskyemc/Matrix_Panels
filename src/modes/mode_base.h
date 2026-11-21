#pragma once

#include <cstdint>
#include "gfx/canvas.h"

namespace core {
struct AppState;
class MediaStore;
}

namespace modes {

enum class ModeId : uint8_t {
    Clock = 0,
    Weather,
    MoonPhase,
    MusicVisualizer,
    Seasonal,
    SunriseSunset,
    AlbumArt,
    Printer3D,
    PixelArt,
    Fireplace,
    Waves,
    Aquarium,
    MotionReactive,
    Mirror,
    HeartRate,
    AirQuality,
    SolarSystem,
    SpinningGlobe,
    COUNT
};

struct ModeInfo {
    ModeId id;
    const char* key;
    const char* label;
    const char* category;
};

const ModeInfo& getModeInfo(ModeId id);
ModeId modeFromKey(const char* key);
constexpr int kModeCount = static_cast<int>(ModeId::COUNT);

class DisplayMode {
public:
    virtual ~DisplayMode() = default;
    virtual ModeId id() const = 0;
    virtual void onEnter(const core::AppState& state) { (void)state; }
    virtual void update(float dtSec, const core::AppState& state, const core::MediaStore& media) {
        (void)dtSec; (void)state; (void)media;
    }
    virtual void render(gfx::Canvas& canvas, const core::AppState& state, const core::MediaStore& media) = 0;
};

} // namespace modes
