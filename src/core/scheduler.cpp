#include "core/scheduler.h"

#include <algorithm>

namespace core {

bool Scheduler::isMinuteInRange(int nowMin, int startMin, int endMin) {
    if (startMin == endMin) return false;
    if (startMin < endMin) {
        return nowMin >= startMin && nowMin < endMin;
    }
    // Spans midnight (e.g. 22:00 -> 07:00)
    return nowMin >= startMin || nowMin < endMin;
}

void Scheduler::nextPage(AppState& state, SchedulerEvents& ev) {
    const Playlist* pl = state.findPlaylist(state.activePlaylistId);
    if (!pl || pl->pages.empty()) return;
    state.currentPlaylistIndex = (state.currentPlaylistIndex + 1) % pl->pages.size();
    const auto& page = pl->pages[state.currentPlaylistIndex];
    if (!page.mediaId.empty() && page.mode == modes::ModeId::PixelArt) {
        state.modeSettings.pixelArtScene = page.mediaId;
    }
    if (!page.sceneVariant.empty()) {
        if (page.mode == modes::ModeId::Seasonal) state.modeSettings.seasonalTheme = page.sceneVariant;
        else if (page.mode == modes::ModeId::MusicVisualizer) state.modeSettings.musicStyle = page.sceneVariant;
        else if (page.mode == modes::ModeId::MotionReactive) state.modeSettings.motionStyle = page.sceneVariant;
    }
    state.activeMode = page.mode;
    pageElapsedSec_ = 0.0f;
    ev.modeChanged = true;
    ev.newMode = page.mode;
    ev.pageChanged = true;
}

void Scheduler::prevPage(AppState& state, SchedulerEvents& ev) {
    const Playlist* pl = state.findPlaylist(state.activePlaylistId);
    if (!pl || pl->pages.empty()) return;
    if (state.currentPlaylistIndex == 0) {
        state.currentPlaylistIndex = pl->pages.size() - 1;
    } else {
        state.currentPlaylistIndex -= 1;
    }
    const auto& page = pl->pages[state.currentPlaylistIndex];
    if (!page.mediaId.empty() && page.mode == modes::ModeId::PixelArt) {
        state.modeSettings.pixelArtScene = page.mediaId;
    }
    state.activeMode = page.mode;
    pageElapsedSec_ = 0.0f;
    ev.modeChanged = true;
    ev.newMode = page.mode;
    ev.pageChanged = true;
}

SchedulerEvents Scheduler::tick(float dtSec, uint32_t nowMs, AppState& state) {
    SchedulerEvents ev;
    ev.newMode = state.activeMode;

    int nowMin = state.live.time.hour * 60 + state.live.time.minute;

    // 1. Evaluate discrete schedule rules once per local minute
    int minuteKey = state.live.time.weekday * 1440 + nowMin;
    if (minuteKey != lastScheduleMinuteKey_) {
        lastScheduleMinuteKey_ = minuteKey;
        uint8_t dayBit = static_cast<uint8_t>(1u << (state.live.time.weekday % 7));
        for (const auto& rule : state.schedules) {
            if (!rule.enabled) continue;
            if ((rule.daysMask & dayBit) == 0) continue;
            if (rule.hour == state.live.time.hour && rule.minute == state.live.time.minute) {
                if (rule.action == "power_on") {
                    state.powerOn = true;
                } else if (rule.action == "power_off") {
                    state.powerOn = false;
                } else if (rule.action == "brightness") {
                    state.brightness = rule.brightness;
                } else if (rule.action == "mode") {
                    state.playlistPlaying = false;
                    modes::ModeId m = modes::modeFromKey(rule.value.c_str());
                    if (m != state.activeMode) {
                        state.activeMode = m;
                        ev.modeChanged = true;
                        ev.newMode = m;
                    }
                    if (rule.brightness > 0) state.brightness = rule.brightness;
                } else if (rule.action == "playlist") {
                    if (state.findPlaylist(rule.value) != nullptr) {
                        state.activePlaylistId = rule.value;
                        state.playlistPlaying = true;
                        state.currentPlaylistIndex = 0;
                        pageElapsedSec_ = 0.0f;
                        const Playlist* pl = state.findPlaylist(state.activePlaylistId);
                        if (pl && !pl->pages.empty()) {
                            state.activeMode = pl->pages[0].mode;
                            ev.modeChanged = true;
                            ev.newMode = state.activeMode;
                            ev.pageChanged = true;
                        }
                    }
                }
            }
        }
    }

    // 2. Evaluate Nighttime Dimming & Automatic Display-Off periods
    const auto& ns = state.nightSchedule;
    int dimStart = ns.dimStartHour * 60 + ns.dimStartMinute;
    int dimEnd   = ns.dimEndHour * 60 + ns.dimEndMinute;
    state.nightDimActive = ns.dimEnabled && isMinuteInRange(nowMin, dimStart, dimEnd);

    int offStart = ns.offStartHour * 60 + ns.offStartMinute;
    int offEnd   = ns.offEndHour * 60 + ns.offEndMinute;
    bool inOffWindow = ns.autoOffEnabled && isMinuteInRange(nowMin, offStart, offEnd);

    bool motionAwake = false;
    if (inOffWindow && ns.wakeOnMotion && state.live.lastMotionMs > 0) {
        uint32_t elapsedMs = (nowMs >= state.live.lastMotionMs) ? (nowMs - state.live.lastMotionMs) : 0;
        if (elapsedMs < static_cast<uint32_t>(ns.wakeDurationSec) * 1000u) {
            motionAwake = true;
        }
    }

    state.autoOffActive = inOffWindow && !motionAwake;
    state.effectivePowerOn = state.powerOn && !state.autoOffActive;

    if (state.nightDimActive) {
        state.effectiveBrightness = std::min(state.brightness, ns.dimBrightness);
    } else {
        state.effectiveBrightness = state.brightness;
    }

    // 3. Advance Playlist Loop (works 100% offline)
    if (state.effectivePowerOn && state.playlistPlaying) {
        const Playlist* pl = state.findPlaylist(state.activePlaylistId);
        if (pl && !pl->pages.empty()) {
            if (state.currentPlaylistIndex >= pl->pages.size()) {
                state.currentPlaylistIndex = 0;
            }
            const auto& curPage = pl->pages[state.currentPlaylistIndex];
            if (state.activeMode != curPage.mode) {
                state.activeMode = curPage.mode;
                if (!curPage.mediaId.empty() && curPage.mode == modes::ModeId::PixelArt) {
                    state.modeSettings.pixelArtScene = curPage.mediaId;
                }
                ev.modeChanged = true;
                ev.newMode = curPage.mode;
            }
            pageElapsedSec_ += dtSec;
            float duration = static_cast<float>(std::max<uint16_t>(3, curPage.durationSec));
            if (pageElapsedSec_ >= duration && pl->pages.size() > 1) {
                nextPage(state, ev);
            }
        }
    }

    return ev;
}

} // namespace core
