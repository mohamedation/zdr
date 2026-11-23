#include "ui/ship_glyph.h"

#include "framebuffer.h"

#include <cmath>

namespace ShipGlyph {
namespace {

// Bow-up ship: pointed bow, beam amidships, flat stern.
constexpr int kPattern[][2] = {
    { 0,  6},                                              // bow tip
    {-1,  5}, { 0,  5}, { 1,  5},
    {-1,  4}, { 0,  4}, { 1,  4},
    {-2,  3}, {-1,  3}, { 0,  3}, { 1,  3}, { 2,  3},
    {-2,  2}, {-1,  2}, { 0,  2}, { 1,  2}, { 2,  2},
    {-3,  1}, {-2,  1}, {-1,  1}, { 0,  1}, { 1,  1}, { 2,  1}, { 3,  1},
    {-3,  0}, {-2,  0}, {-1,  0}, { 0,  0}, { 1,  0}, { 2,  0}, { 3,  0},
    {-3, -1}, {-2, -1}, {-1, -1}, { 0, -1}, { 1, -1}, { 2, -1}, { 3, -1},
    {-3, -2}, {-2, -2}, {-1, -2}, { 0, -2}, { 1, -2}, { 2, -2}, { 3, -2},
    {-2, -3}, {-1, -3}, { 0, -3}, { 1, -3}, { 2, -3},
    {-2, -4}, {-1, -4}, { 0, -4}, { 1, -4}, { 2, -4},      // stern
};
constexpr int kCount = sizeof(kPattern) / sizeof(kPattern[0]);

}  // namespace

void draw(Framebuffer& fb, int cx, int cy, uint16_t color, float heading_deg,
          float scale) {
    if (scale <= 0.0f) return;

    const float rad = (heading_deg - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    const float sin_h = std::sin(rad);
    const float cos_h = std::cos(rad);

    for (int i = 0; i < kCount; ++i) {
        const float dx = static_cast<float>(kPattern[i][0]) * scale;
        const float dy = static_cast<float>(kPattern[i][1]) * scale;
        const int sx =
            cx + static_cast<int>(std::lround(dx * (-sin_h) + dy * cos_h));
        const int sy =
            cy + static_cast<int>(std::lround(dx * cos_h + dy * sin_h));
        fb.drawPixel(sx, sy, color);
    }
}

}  // namespace ShipGlyph
