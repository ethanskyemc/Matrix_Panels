#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "core/app_state.h"
#include "core/astro.h"
#include "core/display_engine.h"
#include "core/media_store.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/transitions.h"

static bool canvasHasVisiblePixels(const gfx::Canvas& c) {
    int lit = 0;
    for (int i = 0; i < gfx::Canvas::N; ++i) {
        auto p = c.data()[i];
        if (p.r > 8 || p.g > 8 || p.b > 8) ++lit;
    }
    return lit >= 8;
}

int main() {
    std::cout << "=== Smart 16x16 Acrylic Matrix C++ Test Suite ===\n";

    // 1. Test offline Moon Phase calculation
    {
        // Known New Moon: 2000-01-06 18:14 UTC = 947182440
        auto nm = core::computeMoonPhase(947182440);
        assert(nm.illumination < 0.02);
        // Half a synodic month later (~14.765 days) -> Full Moon
        auto fm = core::computeMoonPhase(947182440 + static_cast<int64_t>(14.765 * 86400));
        assert(fm.illumination > 0.98);
        std::cout << "[PASS] Offline Moon phase math (New=" << nm.illumination
                  << ", Full=" << fm.illumination << ")\n";
    }

    // 2. Test offline NOAA Sunrise & Sunset calculation
    {
        // San Francisco on summer solstice (June 21, PDT = UTC-7 = -420 min)
        auto st = core::computeSunTimes(2026, 6, 21, 37.7749, -122.4194, -420, 720, 30);
        assert(st.valid);
        assert(st.sunriseMinutes > 330 && st.sunriseMinutes < 380); // ~5:48 AM
        assert(st.sunsetMinutes > 1210 && st.sunsetMinutes < 1260); // ~8:35 PM
        assert(st.isDaytime);
        std::cout << "[PASS] Offline NOAA Sunrise/Sunset ("
                  << core::formatMinutesHHMM(st.sunriseMinutes, false) << " / "
                  << core::formatMinutesHHMM(st.sunsetMinutes, false) << ")\n";
    }

    // 3. Test .mpx MediaStore encode/decode round-trip (animated 2-frame GIF)
    {
        core::MediaAsset asset;
        asset.id = "test_gif";
        asset.name = "Test GIF";
        asset.animated = true;
        core::MediaFrame f1, f2;
        f1.delayMs = 120;
        f1.rgb888.assign(kMatrixPixels * 3, 200);
        f2.delayMs = 150;
        f2.rgb888.assign(kMatrixPixels * 3, 80);
        asset.frames = {f1, f2};

        auto encoded = core::MediaStore::encodeMpx(asset);
        assert(encoded.size() == 10 + 2 * (2 + kMatrixPixels * 3));

        core::MediaStore store;
        assert(store.putMpx("test_gif", "Test GIF", encoded.data(), encoded.size()));
        const auto* got = store.get("test_gif");
        assert(got != nullptr && got->frames.size() == 2 && got->animated);
        assert(got->frames[0].rgb888[0] == 200 && got->frames[1].rgb888[0] == 80);
        std::cout << "[PASS] MPX animated media encode/decode (" << encoded.size() << " bytes)\n";
    }

    // 4. Test all 18 display modes render visibly & animate cleanly while OFFLINE
    {
        core::AppState state;
        state.live.wifiConnected = false; // Explicitly verify offline operation
        state.live.haConnected = false;
        state.live.motionDetected = true;
        state.live.motionIntensity = 0.85f;
        state.live.mirrorFrameValid = true;
        for (int i = 0; i < kMatrixPixels * 3; ++i) {
            state.live.mirrorRgb[i] = static_cast<uint8_t>((i * 7) & 0xFF);
            state.live.albumArtRgb[i] = static_cast<uint8_t>((i * 13) & 0xFF);
        }
        state.live.albumArtValid = true;

        core::MediaStore media;
        core::DisplayEngine engine;

        for (int m = 0; m < modes::kModeCount; ++m) {
            auto id = static_cast<modes::ModeId>(m);
            engine.setMode(id, state, false);
            for (int f = 0; f < 5; ++f) {
                engine.tick(0.033f, f * 33, state, media);
            }
            assert(canvasHasVisiblePixels(engine.output()));
            std::cout << "  [OK] Mode " << (m + 1) << "/" << modes::kModeCount
                      << ": " << modes::getModeInfo(id).label << "\n";
        }
        std::cout << "[PASS] All " << modes::kModeCount << " display modes render & animate offline\n";
    }

    // 5. Test smooth transitions between modes
    {
        gfx::Canvas a, b, out;
        a.fill(gfx::palette::Red);
        b.fill(gfx::palette::Cyan);
        for (int t = 0; t < static_cast<int>(gfx::TransitionType::COUNT); ++t) {
            auto tt = static_cast<gfx::TransitionType>(t);
            gfx::applyTransition(a, b, out, tt, 0.5f);
            assert(canvasHasVisiblePixels(out));
            gfx::applyTransition(a, b, out, tt, 1.0f);
            assert(out.getPixel(8, 8) == gfx::palette::Cyan);
        }
        std::cout << "[PASS] All transition effects verified\n";
    }

    // 6. Test Nighttime Dimming, Auto Display-Off with Motion Wake, and Offline Playlist Loop
    {
        core::AppState state;
        state.powerOn = true;
        state.brightness = 200;
        state.nightSchedule.dimEnabled = true;
        state.nightSchedule.dimStartHour = 22;
        state.nightSchedule.dimEndHour = 7;
        state.nightSchedule.dimBrightness = 45;
        state.nightSchedule.autoOffEnabled = true;
        state.nightSchedule.offStartHour = 23;
        state.nightSchedule.offStartMinute = 30;
        state.nightSchedule.offEndHour = 6;
        state.nightSchedule.offEndMinute = 30;
        state.nightSchedule.wakeOnMotion = true;
        state.nightSchedule.wakeDurationSec = 60;

        core::MediaStore media;
        core::DisplayEngine engine;

        // 22:30 -> Night dim active, display still on
        state.live.time.hour = 22;
        state.live.time.minute = 30;
        engine.tick(0.1f, 1000, state, media);
        assert(state.nightDimActive);
        assert(!state.autoOffActive);
        assert(state.effectivePowerOn);
        assert(state.effectiveBrightness == 45);

        // 01:00 -> Auto display-off active
        state.live.time.hour = 1;
        state.live.time.minute = 0;
        state.live.lastMotionMs = 0;
        engine.tick(0.1f, 5000, state, media);
        assert(state.autoOffActive);
        assert(!state.effectivePowerOn);

        // Motion detected at 01:00 -> Wakes display temporarily!
        state.live.lastMotionMs = 5000;
        engine.tick(0.1f, 5100, state, media);
        assert(!state.autoOffActive);
        assert(state.effectivePowerOn);

        // Playlist loop offline advancement
        state.live.time.hour = 14;
        state.live.time.minute = 0;
        state.playlistPlaying = true;
        state.activePlaylistId = "default";
        state.currentPlaylistIndex = 0;
        engine.scheduler().resetPageTimer();
        auto firstMode = state.playlists[0].pages[0].mode;
        auto secondMode = state.playlists[0].pages[1].mode;
        engine.tick(0.1f, 10000, state, media);
        assert(state.activeMode == firstMode);
        // Advance past page 0 duration (15s)
        for (int i = 0; i < 80; ++i) {
            engine.tick(0.2f, 10000 + i * 200, state, media);
        }
        assert(state.activeMode == secondMode);
        std::cout << "[PASS] Nighttime dimming, auto-off + motion wake, and offline playlist loop\n";
    }

    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
