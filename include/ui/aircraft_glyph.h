#ifndef AIRCRAFT_GLYPH_H
#define AIRCRAFT_GLYPH_H

#include <cstdint>

class Framebuffer;

/**
 * Shared top-down aircraft silhouette used by radar and the vertical profile.
 * Pattern coordinates: dx = right, dy = forward (nose). At heading 0° the
 * nose points up on screen.
 */
namespace AircraftGlyph {

/** Full-size radar pattern point count. */
int patternCount();

/** Draw the silhouette centered at (cx, cy).
 *  @param heading_deg Aircraft heading (0 = north, clockwise)
 *  @param scale       1.0 = radar size; ~0.55 for the vertical profile
 */
void draw(Framebuffer& fb, int cx, int cy, uint16_t color,
          float heading_deg = 0.0f, float scale = 1.0f);

}  // namespace AircraftGlyph

#endif  // AIRCRAFT_GLYPH_H
