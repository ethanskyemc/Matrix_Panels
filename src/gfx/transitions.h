#pragma once

#include <cstdint>
#include "gfx/canvas.h"

namespace gfx {

enum class TransitionType : uint8_t {
    Instant = 0,
    Crossfade,
    SlideLeft,
    SlideUp,
    WipeRight,
    Dissolve,
    Iris,
    COUNT
};

const char* transitionName(TransitionType t);
TransitionType transitionFromName(const char* name);

// Render blended transition between `from` and `to` at progress `t` in [0, 1]
void applyTransition(const Canvas& from, const Canvas& to, Canvas& out,
                     TransitionType type, float t);

} // namespace gfx
