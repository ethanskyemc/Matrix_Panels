#include "gfx/transitions.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gfx {

const char* transitionName(TransitionType t) {
    switch (t) {
        case TransitionType::Instant:   return "instant";
        case TransitionType::Crossfade: return "crossfade";
        case TransitionType::SlideLeft: return "slide_left";
        case TransitionType::SlideUp:   return "slide_up";
        case TransitionType::WipeRight: return "wipe_right";
        case TransitionType::Dissolve:  return "dissolve";
        case TransitionType::Iris:      return "iris";
        default:                        return "crossfade";
    }
}

TransitionType transitionFromName(const char* name) {
    if (!name) return TransitionType::Crossfade;
    if (std::strcmp(name, "instant") == 0)    return TransitionType::Instant;
    if (std::strcmp(name, "crossfade") == 0)  return TransitionType::Crossfade;
    if (std::strcmp(name, "slide_left") == 0) return TransitionType::SlideLeft;
    if (std::strcmp(name, "slide_up") == 0)   return TransitionType::SlideUp;
    if (std::strcmp(name, "wipe_right") == 0) return TransitionType::WipeRight;
    if (std::strcmp(name, "dissolve") == 0)   return TransitionType::Dissolve;
    if (std::strcmp(name, "iris") == 0)       return TransitionType::Iris;
    return TransitionType::Crossfade;
}

static inline float smoothstep(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void applyTransition(const Canvas& from, const Canvas& to, Canvas& out,
                     TransitionType type, float t) {
    float p = smoothstep(t);
    if (type == TransitionType::Instant || p >= 1.0f) {
        out = to;
        return;
    }
    if (p <= 0.0f) {
        out = from;
        return;
    }

    switch (type) {
        case TransitionType::Crossfade: {
            for (int y = 0; y < Canvas::H; ++y) {
                for (int x = 0; x < Canvas::W; ++x) {
                    out.setPixel(x, y, lerpColor(from.getPixel(x, y), to.getPixel(x, y), p));
                }
            }
            break;
        }
        case TransitionType::SlideLeft: {
            float shift = p * Canvas::W;
            int offset = static_cast<int>(shift);
            float frac = shift - offset;
            for (int y = 0; y < Canvas::H; ++y) {
                for (int x = 0; x < Canvas::W; ++x) {
                    int sx = x + offset;
                    RGB c0 = (sx < Canvas::W) ? from.getPixel(sx, y) : to.getPixel(sx - Canvas::W, y);
                    int sx1 = sx + 1;
                    RGB c1 = (sx1 < Canvas::W) ? from.getPixel(sx1, y) : to.getPixel(sx1 - Canvas::W, y);
                    out.setPixel(x, y, lerpColor(c0, c1, frac));
                }
            }
            break;
        }
        case TransitionType::SlideUp: {
            float shift = p * Canvas::H;
            int offset = static_cast<int>(shift);
            float frac = shift - offset;
            for (int y = 0; y < Canvas::H; ++y) {
                int sy = y + offset;
                int sy1 = sy + 1;
                for (int x = 0; x < Canvas::W; ++x) {
                    RGB c0 = (sy < Canvas::H) ? from.getPixel(x, sy) : to.getPixel(x, sy - Canvas::H);
                    RGB c1 = (sy1 < Canvas::H) ? from.getPixel(x, sy1) : to.getPixel(x, sy1 - Canvas::H);
                    out.setPixel(x, y, lerpColor(c0, c1, frac));
                }
            }
            break;
        }
        case TransitionType::WipeRight: {
            float edge = p * (Canvas::W + 2.0f) - 1.0f;
            for (int y = 0; y < Canvas::H; ++y) {
                for (int x = 0; x < Canvas::W; ++x) {
                    float d = std::clamp(edge - static_cast<float>(x), 0.0f, 1.0f);
                    out.setPixel(x, y, lerpColor(from.getPixel(x, y), to.getPixel(x, y), d));
                }
            }
            break;
        }
        case TransitionType::Dissolve: {
            for (int y = 0; y < Canvas::H; ++y) {
                for (int x = 0; x < Canvas::W; ++x) {
                    // Deterministic pseudo-random threshold per pixel (Bayer-like hash)
                    uint32_t h = static_cast<uint32_t>((x * 73856093u) ^ (y * 19349663u));
                    float threshold = static_cast<float>(h % 256u) / 256.0f;
                    float local = std::clamp((p * 1.25f - threshold) * 4.0f, 0.0f, 1.0f);
                    out.setPixel(x, y, lerpColor(from.getPixel(x, y), to.getPixel(x, y), local));
                }
            }
            break;
        }
        case TransitionType::Iris: {
            float maxR = 12.0f;
            float curR = p * maxR;
            for (int y = 0; y < Canvas::H; ++y) {
                for (int x = 0; x < Canvas::W; ++x) {
                    float dx = x - 7.5f;
                    float dy = y - 7.5f;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    float local = std::clamp(curR - dist + 1.0f, 0.0f, 1.0f);
                    out.setPixel(x, y, lerpColor(from.getPixel(x, y), to.getPixel(x, y), local));
                }
            }
            break;
        }
        default:
            out = to;
            break;
    }
}

} // namespace gfx
