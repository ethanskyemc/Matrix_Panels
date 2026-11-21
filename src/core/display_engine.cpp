#include "core/display_engine.h"

#include <algorithm>
#include "core/astro.h"

namespace core {

DisplayEngine::DisplayEngine() {
    for (int i = 0; i < modes::kModeCount; ++i) {
        modes_[i] = modes::createMode(static_cast<modes::ModeId>(i));
    }
}

void DisplayEngine::setMode(modes::ModeId mode, AppState& state, bool useTransition) {
    int idx = static_cast<int>(mode);
    if (idx < 0 || idx >= modes::kModeCount) return;

    state.activeMode = mode;
    if (mode != currentMode_) {
        previousMode_ = currentMode_;
        currentMode_ = mode;
        prevCanvas_ = currentCanvas_;
        modes_[idx]->onEnter(state);

        if (useTransition && state.transition != gfx::TransitionType::Instant && state.transitionMs > 0) {
            transitioning_ = true;
            transitionElapsed_ = 0.0f;
            transitionDuration_ = std::max(0.1f, static_cast<float>(state.transitionMs) / 1000.0f);
        } else {
            transitioning_ = false;
        }
    }
}

void DisplayEngine::nextPlaylistPage(AppState& state) {
    SchedulerEvents ev;
    scheduler_.nextPage(state, ev);
    if (ev.modeChanged) {
        setMode(ev.newMode, state, true);
    }
}

void DisplayEngine::prevPlaylistPage(AppState& state) {
    SchedulerEvents ev;
    scheduler_.prevPage(state, ev);
    if (ev.modeChanged) {
        setMode(ev.newMode, state, true);
    }
}

void DisplayEngine::tick(float dtSec, uint32_t nowMs, AppState& state, const MediaStore& media) {
    dtSec = std::clamp(dtSec, 0.001f, 0.25f);

    // 1. Tick scheduler (playlists, schedules, nighttime dimming, auto-off)
    SchedulerEvents ev = scheduler_.tick(dtSec, nowMs, state);
    if (ev.modeChanged || state.activeMode != currentMode_) {
        setMode(state.activeMode, state, true);
    }

    // 2. Smooth power on/off fade
    float targetPower = state.effectivePowerOn ? 1.0f : 0.0f;
    if (powerFade_ < targetPower) {
        powerFade_ = std::min(1.0f, powerFade_ + dtSec * 3.5f);
    } else if (powerFade_ > targetPower) {
        powerFade_ = std::max(0.0f, powerFade_ - dtSec * 3.5f);
    }

    if (powerFade_ <= 0.001f) {
        outputCanvas_.clear(gfx::palette::Black);
        return;
    }

    // 3. Update & render active mode (and outgoing mode during transition)
    int curIdx = static_cast<int>(currentMode_);
    modes_[curIdx]->update(dtSec, state, media);
    modes_[curIdx]->render(currentCanvas_, state, media);

    if (transitioning_) {
        int prevIdx = static_cast<int>(previousMode_);
        if (prevIdx != curIdx) {
            modes_[prevIdx]->update(dtSec, state, media);
            modes_[prevIdx]->render(prevCanvas_, state, media);
        }
        transitionElapsed_ += dtSec;
        float progress = std::clamp(transitionElapsed_ / transitionDuration_, 0.0f, 1.0f);
        gfx::applyTransition(prevCanvas_, currentCanvas_, outputCanvas_, state.transition, progress);
        if (progress >= 1.0f) {
            transitioning_ = false;
        }
    } else {
        outputCanvas_ = currentCanvas_;
    }

    // 4. Optional gradual dawn/dusk ambient border tint during Sunrise/Sunset windows
    if (state.modeSettings.sunGradualEffect && currentMode_ != modes::ModeId::SunriseSunset) {
        int nowMin = state.live.time.hour * 60 + state.live.time.minute;
        SunTimes st = computeSunTimes(
            state.live.time.year, state.live.time.month, state.live.time.day,
            state.location.latitude, state.location.longitude,
            state.location.tzOffsetMinutes, nowMin,
            state.modeSettings.sunGradualWindowMin
        );
        float glow = std::max(st.dawnGlow, st.duskGlow);
        if (glow > 0.05f) {
            gfx::RGB tint = (st.dawnGlow >= st.duskGlow) ? gfx::RGB(255, 170, 64) : gfx::RGB(255, 96, 54);
            uint8_t alpha = static_cast<uint8_t>(glow * 36.0f);
            for (int x = 0; x < 16; ++x) {
                outputCanvas_.blendPixel(x, 0, tint, alpha);
                outputCanvas_.blendPixel(x, 15, tint, alpha);
            }
        }
    }

    // 5. Apply smooth power fade
    if (powerFade_ < 0.999f) {
        outputCanvas_.scaleBrightness(static_cast<uint8_t>(powerFade_ * 255.0f));
    }
}

} // namespace core
