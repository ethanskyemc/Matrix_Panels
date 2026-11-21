#pragma once

#include <cstdint>
#include "core/app_state.h"

namespace core {

struct SchedulerEvents {
    bool modeChanged = false;
    modes::ModeId newMode = modes::ModeId::Clock;
    bool pageChanged = false;
};

class Scheduler {
public:
    Scheduler() = default;

    // Advance playlist timer, evaluate schedules, nighttime dimming, and auto display-off
    SchedulerEvents tick(float dtSec, uint32_t nowMs, AppState& state);

    void resetPageTimer() { pageElapsedSec_ = 0.0f; }
    float pageElapsedSec() const { return pageElapsedSec_; }
    void nextPage(AppState& state, SchedulerEvents& ev);
    void prevPage(AppState& state, SchedulerEvents& ev);

    static bool isMinuteInRange(int nowMin, int startMin, int endMin);

private:
    float pageElapsedSec_ = 0.0f;
    int lastScheduleMinuteKey_ = -1;
};

} // namespace core
