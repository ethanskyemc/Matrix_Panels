#pragma once

#include <cstdint>
#include <string>

namespace core {

struct MoonInfo {
    double phase = 0.0;        // 0.0 = New, 0.25 = First Quarter, 0.5 = Full, 0.75 = Last Quarter
    double illumination = 0.0; // 0.0 .. 1.0
    double ageDays = 0.0;      // 0 .. 29.53
    const char* name = "New Moon";
};

struct SunTimes {
    bool valid = true;
    int sunriseMinutes = 405;  // Minutes since midnight local time (e.g., 6:45 = 405)
    int sunsetMinutes = 1085;  // Minutes since midnight local time (e.g., 18:05 = 1085)
    float dayProgress = 0.5f;  // 0.0 at sunrise .. 1.0 at sunset (clamped)
    bool isDaytime = true;
    // Gradual lighting intensity (0 = none, 1 = peak dawn/dusk glow)
    float dawnGlow = 0.0f;
    float duskGlow = 0.0f;
};

// Calculate accurate lunar phase from Unix UTC timestamp (100% offline)
MoonInfo computeMoonPhase(int64_t unixEpochUtc);

// Calculate local sunrise & sunset using NOAA solar algorithm (100% offline)
SunTimes computeSunTimes(int year, int month, int day,
                         double latitudeDeg, double longitudeDeg,
                         int tzOffsetMinutes,
                         int currentLocalMinutes,
                         int gradualWindowMin = 30);

std::string formatMinutesHHMM(int totalMinutes, bool use24h);

// Offline US 5-digit ZIP code -> approximate (lat, lon, standard UTC offset, region)
bool lookupUsZipOffline(const std::string& zipCode,
                        double& outLat, double& outLon,
                        int& outTzOffsetMin, std::string& outLabel);

} // namespace core
