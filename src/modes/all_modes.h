#pragma once

#include <memory>
#include "modes/mode_base.h"

namespace modes {

std::unique_ptr<DisplayMode> createClockMode();
std::unique_ptr<DisplayMode> createWeatherMode();
std::unique_ptr<DisplayMode> createMoonMode();
std::unique_ptr<DisplayMode> createMusicMode();
std::unique_ptr<DisplayMode> createSeasonalMode();
std::unique_ptr<DisplayMode> createSunMode();
std::unique_ptr<DisplayMode> createAlbumMode();
std::unique_ptr<DisplayMode> createPrinterMode();
std::unique_ptr<DisplayMode> createPixelArtMode();
std::unique_ptr<DisplayMode> createFireplaceMode();
std::unique_ptr<DisplayMode> createWavesMode();
std::unique_ptr<DisplayMode> createAquariumMode();
std::unique_ptr<DisplayMode> createMotionMode();
std::unique_ptr<DisplayMode> createMirrorMode();
std::unique_ptr<DisplayMode> createHeartRateMode();
std::unique_ptr<DisplayMode> createAirQualityMode();
std::unique_ptr<DisplayMode> createSolarSystemMode();
std::unique_ptr<DisplayMode> createSpinningGlobeMode();

std::unique_ptr<DisplayMode> createMode(ModeId id);

} // namespace modes
