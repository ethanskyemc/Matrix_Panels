#include "gfx/font.h"

#include <cctype>
#include <cmath>

namespace gfx {

namespace {

// Each 3x5 glyph is 5 rows of 3 bits (MSB = left pixel, bit 2..0)
struct Glyph3x5 {
    char ch;
    uint8_t rows[5];
};

static constexpr Glyph3x5 kFont3x5[] = {
    {'0', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'1', {0b010, 0b110, 0b010, 0b010, 0b111}},
    {'2', {0b111, 0b001, 0b111, 0b100, 0b111}},
    {'3', {0b111, 0b001, 0b111, 0b001, 0b111}},
    {'4', {0b101, 0b101, 0b111, 0b001, 0b001}},
    {'5', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'6', {0b111, 0b100, 0b111, 0b101, 0b111}},
    {'7', {0b111, 0b001, 0b010, 0b010, 0b010}},
    {'8', {0b111, 0b101, 0b111, 0b101, 0b111}},
    {'9', {0b111, 0b101, 0b111, 0b001, 0b111}},

    {'A', {0b010, 0b101, 0b111, 0b101, 0b101}},
    {'B', {0b110, 0b101, 0b110, 0b101, 0b110}},
    {'C', {0b011, 0b100, 0b100, 0b100, 0b011}},
    {'D', {0b110, 0b101, 0b101, 0b101, 0b110}},
    {'E', {0b111, 0b100, 0b110, 0b100, 0b111}},
    {'F', {0b111, 0b100, 0b110, 0b100, 0b100}},
    {'G', {0b011, 0b100, 0b101, 0b101, 0b011}},
    {'H', {0b101, 0b101, 0b111, 0b101, 0b101}},
    {'I', {0b111, 0b010, 0b010, 0b010, 0b111}},
    {'J', {0b001, 0b001, 0b001, 0b101, 0b010}},
    {'K', {0b101, 0b101, 0b110, 0b101, 0b101}},
    {'L', {0b100, 0b100, 0b100, 0b100, 0b111}},
    {'M', {0b101, 0b111, 0b111, 0b101, 0b101}},
    {'N', {0b110, 0b101, 0b101, 0b101, 0b101}},
    {'O', {0b010, 0b101, 0b101, 0b101, 0b010}},
    {'P', {0b110, 0b101, 0b110, 0b100, 0b100}},
    {'Q', {0b010, 0b101, 0b101, 0b111, 0b011}},
    {'R', {0b110, 0b101, 0b110, 0b101, 0b101}},
    {'S', {0b011, 0b100, 0b010, 0b001, 0b110}},
    {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
    {'U', {0b101, 0b101, 0b101, 0b101, 0b111}},
    {'V', {0b101, 0b101, 0b101, 0b101, 0b010}},
    {'W', {0b101, 0b101, 0b111, 0b111, 0b101}},
    {'X', {0b101, 0b101, 0b010, 0b101, 0b101}},
    {'Y', {0b101, 0b101, 0b010, 0b010, 0b010}},
    {'Z', {0b111, 0b001, 0b010, 0b100, 0b111}},

    {'-', {0b000, 0b000, 0b111, 0b000, 0b000}},
    {'+', {0b000, 0b010, 0b111, 0b010, 0b000}},
    {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
    {'.', {0b000, 0b000, 0b000, 0b000, 0b010}},
    {'/', {0b001, 0b001, 0b010, 0b100, 0b100}},
    {'%', {0b101, 0b001, 0b010, 0b100, 0b101}},
    {'!', {0b010, 0b010, 0b010, 0b000, 0b010}},
    {'?', {0b110, 0b001, 0b010, 0b000, 0b010}},
    {'*', {0b010, 0b101, 0b010, 0b000, 0b000}}, // degree-like symbol
    {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
};

// Bold 4x6 digits (4 bits wide, 6 rows)
static constexpr uint8_t kDigits4x6[10][6] = {
    {0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}, // 0
    {0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0111}, // 1
    {0b0110, 0b1001, 0b0010, 0b0100, 0b1000, 0b1111}, // 2
    {0b1110, 0b0001, 0b0110, 0b0001, 0b0001, 0b1110}, // 3
    {0b1001, 0b1001, 0b1111, 0b0001, 0b0001, 0b0001}, // 4
    {0b1111, 0b1000, 0b1110, 0b0001, 0b0001, 0b1110}, // 5
    {0b0111, 0b1000, 0b1110, 0b1001, 0b1001, 0b0110}, // 6
    {0b1111, 0b0001, 0b0010, 0b0100, 0b0100, 0b0100}, // 7
    {0b0110, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110}, // 8
    {0b0110, 0b1001, 0b0111, 0b0001, 0b0001, 0b1110}, // 9
};

int glyphWidth(char ch) {
    if (ch == ':' || ch == '.' || ch == '!') return 1;
    if (ch == ' ') return 2;
    return 3;
}

const uint8_t* findGlyph3x5(char ch) {
    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    for (const auto& g : kFont3x5) {
        if (g.ch == up) return g.rows;
    }
    return nullptr;
}

} // namespace

void drawChar3x5(Canvas& c, int x, int y, char ch, RGB color) {
    const uint8_t* rows = findGlyph3x5(ch);
    if (!rows) return;
    int gw = glyphWidth(ch);
    int startBit = (gw == 1) ? 1 : 2;
    int endBit = (gw == 1) ? 1 : 0;
    for (int r = 0; r < 5; ++r) {
        int px = 0;
        for (int b = startBit; b >= endBit; --b, ++px) {
            if (rows[r] & (1 << b)) {
                c.setPixel(x + px, y + r, color);
            }
        }
    }
}

int measureText3x5(const std::string& text, int spacing) {
    if (text.empty()) return 0;
    int w = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        w += glyphWidth(text[i]);
        if (i + 1 < text.size()) w += spacing;
    }
    return w;
}

void drawText3x5(Canvas& c, int x, int y, const std::string& text, RGB color, int spacing) {
    int cx = x;
    for (char ch : text) {
        drawChar3x5(c, cx, y, ch, color);
        cx += glyphWidth(ch) + spacing;
    }
}

void drawTextCentered3x5(Canvas& c, int y, const std::string& text, RGB color, int spacing) {
    int w = measureText3x5(text, spacing);
    int x = (Canvas::W - w) / 2;
    drawText3x5(c, x, y, text, color, spacing);
}

void drawDigit4x6(Canvas& c, int x, int y, uint8_t digit, RGB color) {
    if (digit > 9) return;
    for (int r = 0; r < 6; ++r) {
        uint8_t row = kDigits4x6[digit][r];
        for (int col = 0; col < 4; ++col) {
            if (row & (1 << (3 - col))) {
                c.setPixel(x + col, y + r, color);
            }
        }
    }
}

void drawTwoDigits4x6(Canvas& c, int x, int y, int value, RGB color, bool leadingZero) {
    int v = std::abs(value) % 100;
    uint8_t tens = static_cast<uint8_t>(v / 10);
    uint8_t ones = static_cast<uint8_t>(v % 10);
    if (tens > 0 || leadingZero) {
        drawDigit4x6(c, x, y, tens, color);
    }
    drawDigit4x6(c, x + 5, y, ones, color);
}

void drawMarquee3x5(Canvas& c, int y, const std::string& text, RGB color, float scrollPx) {
    int tw = measureText3x5(text, 1);
    if (tw <= Canvas::W) {
        drawTextCentered3x5(c, y, text, color, 1);
        return;
    }
    int cycle = tw + Canvas::W + 4;
    int offset = static_cast<int>(std::fmod(scrollPx, static_cast<float>(cycle)));
    int startX = Canvas::W - offset;
    drawText3x5(c, startX, y, text, color, 1);
}

void drawDegreeDot(Canvas& c, int x, int y, RGB color) {
    c.setPixel(x, y, color);
    c.setPixel(x + 1, y, color);
    c.setPixel(x, y + 1, color);
    c.setPixel(x + 1, y + 1, color);
}

} // namespace gfx
