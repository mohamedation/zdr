#ifndef AIRCRAFT_GLYPH_H
#define AIRCRAFT_GLYPH_H

#include <cstdint>

class Framebuffer;

/**
 * Top-down aircraft mark for radar / profile / detail.
 *
 * scale ≲ 1.8 → filled ATC dart (crisp on the scope).
 * scale ≳ 1.8 → filled swept-wing jet (detail rose).
 * Heading 0° = nose up on screen.
 */
namespace AircraftGlyph {

int patternCount();

void draw(Framebuffer& fb, int cx, int cy, uint16_t color,
          float heading_deg = 0.0f, float scale = 1.0f);

}  // namespace AircraftGlyph

#endif  // AIRCRAFT_GLYPH_H
