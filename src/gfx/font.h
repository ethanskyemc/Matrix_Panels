#pragma once

#include <cstdint>
#include <string>
#include "gfx/canvas.h"

namespace gfx {

// Compact 3x5 pixel font — fits up to 4 characters across 16px (e.g. "72°", "100%", "12:45")
void drawChar3x5(Canvas& c, int x, int y, char ch, RGB color);
int measureText3x5(const std::string& text, int spacing = 1);
void drawText3x5(Canvas& c, int x, int y, const std::string& text, RGB color, int spacing = 1);
void drawTextCentered3x5(Canvas& c, int y, const std::string& text, RGB color, int spacing = 1);

// Bold 4x6 digit font for ultra-readable stacked clock displays (2 digits = 9px wide, fits cleanly in 16px)
void drawDigit4x6(Canvas& c, int x, int y, uint8_t digit, RGB color);
void drawTwoDigits4x6(Canvas& c, int x, int y, int value, RGB color, bool leadingZero = true);

// Horizontal scrolling marquee text using 3x5 font
void drawMarquee3x5(Canvas& c, int y, const std::string& text, RGB color, float scrollPx);

// Draw tiny degree symbol (2x2 hollow or 2x2 dot depending on context)
void drawDegreeDot(Canvas& c, int x, int y, RGB color);

} // namespace gfx
