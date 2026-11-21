#include "core/astro.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace core {

namespace {
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDeg2Rad = kPi / 180.0;
static constexpr double kRad2Deg = 180.0 / kPi;

int dayOfYear(int year, int month, int day) {
    static constexpr int kMonthDays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int doy = day;
    for (int m = 1; m < month && m <= 12; ++m) {
        doy += kMonthDays[m - 1];
        if (m == 2 && leap) doy += 1;
    }
    return doy;
}
} // namespace

MoonInfo computeMoonPhase(int64_t unixEpochUtc) {
    // Known new moon reference: 2000-01-06 18:14 UTC = Unix 947182440
    static constexpr double kKnownNewMoon = 947182440.0;
    static constexpr double kSynodicMonthSec = 29.53058867 * 86400.0;

    double diff = static_cast<double>(unixEpochUtc) - kKnownNewMoon;
    double cycles = diff / kSynodicMonthSec;
    double phase = cycles - std::floor(cycles);
    if (phase < 0.0) phase += 1.0;

    MoonInfo info;
    info.phase = phase;
    info.ageDays = phase * 29.53058867;
    info.illumination = 0.5 * (1.0 - std::cos(2.0 * kPi * phase));

    if (phase < 0.03 || phase >= 0.97)      info.name = "New Moon";
    else if (phase < 0.22)                  info.name = "Waxing Crescent";
    else if (phase < 0.28)                  info.name = "First Quarter";
    else if (phase < 0.47)                  info.name = "Waxing Gibbous";
    else if (phase < 0.53)                  info.name = "Full Moon";
    else if (phase < 0.72)                  info.name = "Waning Gibbous";
    else if (phase < 0.78)                  info.name = "Last Quarter";
    else                                    info.name = "Waning Crescent";

    return info;
}

SunTimes computeSunTimes(int year, int month, int day,
                         double latitudeDeg, double longitudeDeg,
                         int tzOffsetMinutes,
                         int currentLocalMinutes,
                         int gradualWindowMin) {
    SunTimes res;
    int n = dayOfYear(year, month, day);

    // Fractional year (radians)
    double gamma = (2.0 * kPi / 365.0) * (n - 1);

    // Equation of time (minutes)
    double eqTime = 229.18 * (0.000075
                    + 0.001868 * std::cos(gamma)
                    - 0.032077 * std::sin(gamma)
                    - 0.014615 * std::cos(2.0 * gamma)
                    - 0.040849 * std::sin(2.0 * gamma));

    // Solar declination (radians)
    double decl = 0.006918
                  - 0.399912 * std::cos(gamma)
                  + 0.070257 * std::sin(gamma)
                  - 0.006758 * std::cos(2.0 * gamma)
                  + 0.000907 * std::sin(2.0 * gamma)
                  - 0.002697 * std::cos(3.0 * gamma)
                  + 0.001480 * std::sin(3.0 * gamma);

    double latRad = latitudeDeg * kDeg2Rad;
    double zenithRad = 90.833 * kDeg2Rad;
    double cosHa = (std::cos(zenithRad) / (std::cos(latRad) * std::cos(decl)))
                   - std::tan(latRad) * std::tan(decl);

    if (cosHa > 1.0 || cosHa < -1.0) {
        // Polar night or midnight sun
        res.valid = false;
        res.sunriseMinutes = 360;
        res.sunsetMinutes = 1080;
        res.isDaytime = (cosHa < -1.0);
        res.dayProgress = res.isDaytime ? 0.5f : 0.0f;
        return res;
    }

    double haDeg = std::acos(cosHa) * kRad2Deg;

    // Sunrise and sunset in UTC minutes, then shifted to local minutes
    double sunriseUtc = 720.0 - 4.0 * (longitudeDeg + haDeg) - eqTime;
    double sunsetUtc  = 720.0 - 4.0 * (longitudeDeg - haDeg) - eqTime;

    auto wrapDay = [](double m) -> int {
        while (m < 0.0) m += 1440.0;
        while (m >= 1440.0) m -= 1440.0;
        return static_cast<int>(m + 0.5);
    };

    res.valid = true;
    res.sunriseMinutes = wrapDay(sunriseUtc + tzOffsetMinutes);
    res.sunsetMinutes = wrapDay(sunsetUtc + tzOffsetMinutes);

    if (currentLocalMinutes >= res.sunriseMinutes && currentLocalMinutes <= res.sunsetMinutes) {
        res.isDaytime = true;
        int span = std::max(1, res.sunsetMinutes - res.sunriseMinutes);
        res.dayProgress = static_cast<float>(currentLocalMinutes - res.sunriseMinutes) / static_cast<float>(span);
    } else {
        res.isDaytime = false;
        res.dayProgress = (currentLocalMinutes < res.sunriseMinutes) ? 0.0f : 1.0f;
    }

    // Gradual dawn/dusk lighting effect intensity around sunrise and sunset
    if (gradualWindowMin > 0) {
        float win = static_cast<float>(gradualWindowMin);
        float dRise = std::fabs(static_cast<float>(currentLocalMinutes - res.sunriseMinutes));
        float dSet  = std::fabs(static_cast<float>(currentLocalMinutes - res.sunsetMinutes));
        if (dRise < win) {
            res.dawnGlow = 1.0f - (dRise / win);
        }
        if (dSet < win) {
            res.duskGlow = 1.0f - (dSet / win);
        }
    }

    return res;
}

std::string formatMinutesHHMM(int totalMinutes, bool use24h) {
    int m = ((totalMinutes % 1440) + 1440) % 1440;
    int h = m / 60;
    int min = m % 60;
    if (!use24h) {
        h = h % 12;
        if (h == 0) h = 12;
    }
    char buf[8];
    if (use24h) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", h, min);
    } else {
        std::snprintf(buf, sizeof(buf), "%d:%02d", h, min);
    }
    return std::string(buf);
}

bool lookupUsZipOffline(const std::string& zipCode,
                        double& outLat, double& outLon,
                        int& outTzOffsetMin, std::string& outLabel) {
    // Extract first 5 digits
    std::string digits;
    for (char c : zipCode) {
        if (c >= '0' && c <= '9') digits.push_back(c);
    }
    if (digits.size() < 5) return false;
    int z = std::atoi(digits.substr(0, 5).c_str());

    struct ZipRegion { int minZ; int maxZ; double lat; double lon; int tzMin; const char* label; };
    static constexpr ZipRegion kRegions[] = {
        {  600,   999, 18.46,  -66.11, -240, "Puerto Rico"},
        { 1000,  2799, 42.36,  -71.06, -300, "Massachusetts"},
        { 2800,  2999, 41.82,  -71.41, -300, "Rhode Island"},
        { 3000,  3899, 43.21,  -71.54, -300, "New Hampshire"},
        { 3900,  4999, 43.66,  -70.26, -300, "Maine"},
        { 5000,  5999, 44.26,  -72.58, -300, "Vermont"},
        { 6000,  6999, 41.76,  -72.68, -300, "Connecticut"},
        { 7000,  8999, 40.74,  -74.17, -300, "New Jersey"},
        {10000, 14999, 40.71,  -74.01, -300, "New York"},
        {15000, 19699, 40.27,  -76.88, -300, "Pennsylvania"},
        {19700, 19999, 39.74,  -75.55, -300, "Delaware"},
        {20000, 20599, 38.91,  -77.04, -300, "Washington, DC"},
        {20600, 21999, 39.29,  -76.61, -300, "Maryland"},
        {22000, 24699, 37.54,  -77.44, -300, "Virginia"},
        {24700, 26899, 38.35,  -81.63, -300, "West Virginia"},
        {27000, 28999, 35.78,  -78.64, -300, "North Carolina"},
        {29000, 29999, 34.00,  -81.03, -300, "South Carolina"},
        {30000, 31999, 33.75,  -84.39, -300, "Georgia"},
        {32000, 34999, 28.54,  -81.38, -300, "Florida"},
        {35000, 36999, 33.52,  -86.81, -360, "Alabama"},
        {37000, 38599, 36.16,  -86.78, -360, "Tennessee"},
        {38600, 39799, 32.30,  -90.18, -360, "Mississippi"},
        {40000, 42799, 38.25,  -85.76, -300, "Kentucky"},
        {43000, 45999, 39.96,  -83.00, -300, "Ohio"},
        {46000, 47999, 39.77,  -86.16, -300, "Indiana"},
        {48000, 49999, 42.33,  -83.05, -300, "Michigan"},
        {50000, 52899, 41.59,  -93.62, -360, "Iowa"},
        {53000, 54999, 43.04,  -87.91, -360, "Wisconsin"},
        {55000, 56799, 44.98,  -93.27, -360, "Minnesota"},
        {57000, 57799, 43.55,  -96.73, -360, "South Dakota"},
        {58000, 58899, 46.81, -100.78, -360, "North Dakota"},
        {59000, 59999, 46.59, -112.03, -420, "Montana"},
        {60000, 62999, 41.88,  -87.63, -360, "Illinois"},
        {63000, 65899, 38.63,  -90.20, -360, "Missouri"},
        {66000, 67999, 39.05,  -95.68, -360, "Kansas"},
        {68000, 69399, 41.26,  -95.93, -360, "Nebraska"},
        {70000, 71499, 29.95,  -90.07, -360, "Louisiana"},
        {71600, 72999, 34.75,  -92.29, -360, "Arkansas"},
        {73000, 74999, 35.47,  -97.52, -360, "Oklahoma"},
        {75000, 79999, 30.27,  -97.74, -360, "Texas"},
        {80000, 81699, 39.74, -104.99, -420, "Colorado"},
        {82000, 83199, 41.14, -104.82, -420, "Wyoming"},
        {83200, 83899, 43.62, -116.20, -420, "Idaho"},
        {84000, 84799, 40.76, -111.89, -420, "Utah"},
        {85000, 86599, 33.45, -112.07, -420, "Arizona"},
        {87000, 88499, 35.08, -106.65, -420, "New Mexico"},
        {88900, 89899, 36.17, -115.14, -480, "Nevada"},
        {90000, 93599, 34.05, -118.24, -480, "Southern CA"},
        {93600, 96199, 37.77, -122.42, -480, "Northern CA"},
        {96700, 96899, 21.31, -157.86, -600, "Hawaii"},
        {97000, 97999, 45.52, -122.68, -480, "Oregon"},
        {98000, 99499, 47.61, -122.33, -480, "Washington"},
        {99500, 99999, 61.22, -149.90, -540, "Alaska"},
    };

    for (const auto& r : kRegions) {
        if (z >= r.minZ && z <= r.maxZ) {
            outLat = r.lat;
            outLon = r.lon;
            outTzOffsetMin = r.tzMin;
            outLabel = std::string(r.label) + " (" + digits.substr(0, 5) + ")";
            return true;
        }
    }
    return false;
}

} // namespace core
