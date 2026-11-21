#pragma once

#include <array>
#include <memory>
#include "core/app_state.h"
#include "core/media_store.h"
#include "core/scheduler.h"
#include "gfx/canvas.h"
#include "modes/all_modes.h"

namespace core {

class DisplayEngine {
public:
    DisplayEngine();

    void setMode(modes::ModeId mode, AppState& state, bool useTransition = true);
    void nextPlaylistPage(AppState& state);
    void prevPlaylistPage(AppState& state);

    // Advance engine by dtSec and render into internal output canvas
    void tick(float dtSec, uint32_t nowMs, AppState& state, const MediaStore& media);

    const gfx::Canvas& output() const { return outputCanvas_; }
    const Scheduler& scheduler() const { return scheduler_; }
    Scheduler& scheduler() { return scheduler_; }

private:
    std::array<std::unique_ptr<modes::DisplayMode>, modes::kModeCount> modes_;
    Scheduler scheduler_;

    modes::ModeId currentMode_ = modes::ModeId::Clock;
    modes::ModeId previousMode_ = modes::ModeId::Clock;
    bool transitioning_ = false;
    float transitionElapsed_ = 0.0f;
    float transitionDuration_ = 0.65f;

    gfx::Canvas currentCanvas_;
    gfx::Canvas prevCanvas_;
    gfx::Canvas outputCanvas_;
    float powerFade_ = 1.0f; // Smooth power on/off fade
};

} // namespace core
