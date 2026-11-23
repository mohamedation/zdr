#include "ui/aircraft_glyph.h"

#include "framebuffer.h"

#include <cmath>

namespace AircraftGlyph {
namespace {

// Top-down jet silhouette (nose = +dy). Dense enough that scale≈1 radar
// blips read clearly; larger detail scales stamp each point so wings stay solid.
constexpr int kPattern[][2] = {
    // Nose / cockpit
    { 0,  6},
    {-1,  5}, { 0,  5}, { 1,  5},
    {-1,  4}, { 0,  4}, { 1,  4},
    // Forward fuselage
    {-1,  3}, { 0,  3}, { 1,  3},
    {-1,  2}, { 0,  2}, { 1,  2},
    // Wing leading edge + engines
    {-5,  1}, {-4,  1}, {-3,  1}, {-2,  1}, {-1,  1}, { 0,  1},
    { 1,  1}, { 2,  1}, { 3,  1}, { 4,  1}, { 5,  1},
    // Main wing (full span)
    {-6,  0}, {-5,  0}, {-4,  0}, {-3,  0}, {-2,  0}, {-1,  0}, { 0,  0},
    { 1,  0}, { 2,  0}, { 3,  0}, { 4,  0}, { 5,  0}, { 6,  0},
    {-5, -1}, {-4, -1}, {-3, -1}, {-2, -1}, {-1, -1}, { 0, -1},
    { 1, -1}, { 2, -1}, { 3, -1}, { 4, -1}, { 5, -1},
    // Aft fuselage
    {-1, -2}, { 0, -2}, { 1, -2},
    {-1, -3}, { 0, -3}, { 1, -3},
    // Horizontal stabilizer
    {-3, -4}, {-2, -4}, {-1, -4}, { 0, -4}, { 1, -4}, { 2, -4}, { 3, -4},
    {-2, -5}, {-1, -5}, { 0, -5}, { 1, -5}, { 2, -5},
    // Tail tip / fin
    { 0, -6},
};
constexpr int kCount = sizeof(kPattern) / sizeof(kPattern[0]);

void stamp(Framebuffer& fb, int x, int y, uint16_t color, int radius) {
    if (radius <= 1) {
        fb.drawPixel(x, y, color);
        return;
    }
    const int r2 = radius * radius;
    for (int dy = -radius + 1; dy < radius; ++dy) {
        for (int dx = -radius + 1; dx < radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                fb.drawPixel(x + dx, y + dy, color);
            }
        }
    }
}

}  // namespace

int patternCount() { return kCount; }

void draw(Framebuffer& fb, int cx, int cy, uint16_t color, float heading_deg,
          float scale) {
    if (scale <= 0.0f) return;

    const float rad = (heading_deg - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    const float sin_h = std::sin(rad);
    const float cos_h = std::cos(rad);
    // Larger scales stamp discs so rotated wings don't dissolve into dots.
    const int brush =
        (scale > 1.15f) ? std::max(2, static_cast<int>(std::lround(scale * 0.7f)))
                        : 1;

    for (int i = 0; i < kCount; ++i) {
        const float dx = static_cast<float>(kPattern[i][0]) * scale;
        const float dy = static_cast<float>(kPattern[i][1]) * scale;
        const int sx =
            cx + static_cast<int>(std::lround(dx * (-sin_h) + dy * cos_h));
        const int sy =
            cy + static_cast<int>(std::lround(dx * cos_h + dy * sin_h));
        stamp(fb, sx, sy, color, brush);
    }
}

}  // namespace AircraftGlyph
