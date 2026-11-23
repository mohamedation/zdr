#include "ui/aircraft_glyph.h"

#include "framebuffer.h"

#include <cmath>

namespace AircraftGlyph {
namespace {

// Aircraft silhouette (11 tall, 9 wide), nose in +dy.
// Pointed nose, tapering body, full-width wing at y=-1, narrow tail.
constexpr int kPattern[][2] = {
    { 0,  5},                                              // nose tip
    { 0,  4},                                              // nose
    { 0,  3},                                              // nose
    {-1,  2}, { 0,  2}, { 1,  2},                          // fuselage
    {-2,  1}, {-1,  1}, { 0,  1}, { 1,  1}, { 2,  1},     // fuselage
    {-3,  0}, {-2,  0}, {-1,  0}, { 0,  0},                // wing leading edge
    { 1,  0}, { 2,  0}, { 3,  0},                          // wing leading edge
    {-4, -1}, {-3, -1}, {-2, -1}, {-1, -1}, { 0, -1},      // wing full span
    { 1, -1}, { 2, -1}, { 3, -1}, { 4, -1},                // wing full span
    { 0, -2},                                              // fuselage
    { 0, -3},                                              // fuselage
    {-2, -4}, {-1, -4}, { 0, -4}, { 1, -4}, { 2, -4},      // tail
    { 0, -5},                                              // tail tip
};
constexpr int kCount = sizeof(kPattern) / sizeof(kPattern[0]);

}  // namespace

int patternCount() { return kCount; }

void draw(Framebuffer& fb, int cx, int cy, uint16_t color, float heading_deg,
          float scale) {
    if (scale <= 0.0f) return;

    const float rad = (heading_deg - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    const float sin_h = std::sin(rad);
    const float cos_h = std::cos(rad);
    // right = (-sin_h, cos_h); forward = (cos_h, sin_h)

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

}  // namespace AircraftGlyph
