#include "modes/all_modes.h"

#include <cstring>

namespace modes {

static constexpr ModeInfo kModeInfos[] = {
    {ModeId::Clock,           "clock",            "Clock",            "Utility"},
    {ModeId::Weather,         "weather",          "Weather",          "Info"},
    {ModeId::MoonPhase,       "moon_phase",       "Moon Phase",       "Astronomy"},
    {ModeId::MusicVisualizer, "music_visualizer", "Music Visualizer", "Reactive"},
    {ModeId::Seasonal,        "seasonal",         "Seasonal Themes",  "Ambient"},
    {ModeId::SunriseSunset,   "sunrise_sunset",   "Sunrise & Sunset", "Astronomy"},
    {ModeId::AlbumArt,        "album_art",        "Album Art",        "Media"},
    {ModeId::Printer3D,       "printer_3d",       "3D Printer",       "Smart Home"},
    {ModeId::PixelArt,        "pixel_art",        "Pixel Art",        "Art"},
    {ModeId::Fireplace,       "fireplace",        "Fireplace",        "Ambient"},
    {ModeId::Waves,           "waves",            "Ocean Waves",      "Ambient"},
    {ModeId::Aquarium,        "aquarium",         "Aquarium",         "Ambient"},
    {ModeId::MotionReactive,  "motion_reactive",  "Motion Reactive",  "Reactive"},
    {ModeId::Mirror,          "mirror",           "Mirror Mode",      "Reactive"},
    {ModeId::HeartRate,       "heart_rate",       "Heart Rate",       "Sensors"},
    {ModeId::AirQuality,      "air_quality",      "Air Quality",      "Sensors"},
    {ModeId::SolarSystem,     "solar_system",     "Solar System",     "Space"},
    {ModeId::SpinningGlobe,   "spinning_globe",   "Spinning Globe",   "Space"},
};

static_assert(sizeof(kModeInfos) / sizeof(kModeInfos[0]) == kModeCount,
              "kModeInfos must match ModeId::COUNT");

const ModeInfo& getModeInfo(ModeId id) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= kModeCount) idx = 0;
    return kModeInfos[idx];
}

ModeId modeFromKey(const char* key) {
    if (!key) return ModeId::Clock;
    for (const auto& info : kModeInfos) {
        if (std::strcmp(info.key, key) == 0) return info.id;
    }
    return ModeId::Clock;
}

std::unique_ptr<DisplayMode> createMode(ModeId id) {
    switch (id) {
        case ModeId::Clock:           return createClockMode();
        case ModeId::Weather:         return createWeatherMode();
        case ModeId::MoonPhase:       return createMoonMode();
        case ModeId::MusicVisualizer: return createMusicMode();
        case ModeId::Seasonal:        return createSeasonalMode();
        case ModeId::SunriseSunset:   return createSunMode();
        case ModeId::AlbumArt:        return createAlbumMode();
        case ModeId::Printer3D:       return createPrinterMode();
        case ModeId::PixelArt:        return createPixelArtMode();
        case ModeId::Fireplace:       return createFireplaceMode();
        case ModeId::Waves:           return createWavesMode();
        case ModeId::Aquarium:        return createAquariumMode();
        case ModeId::MotionReactive:  return createMotionMode();
        case ModeId::Mirror:          return createMirrorMode();
        case ModeId::HeartRate:       return createHeartRateMode();
        case ModeId::AirQuality:      return createAirQualityMode();
        case ModeId::SolarSystem:     return createSolarSystemMode();
        case ModeId::SpinningGlobe:   return createSpinningGlobeMode();
        default:                      return createClockMode();
    }
}

} // namespace modes
