#ifndef SHIP_GLYPH_H
#define SHIP_GLYPH_H

#include <cstdint>

class Framebuffer;

/**
 * Top-down ship silhouette for the AIS dossier heading rose.
 * Pattern coordinates: dx = starboard, dy = forward (bow). At heading 0°
 * the bow points up on screen.
 */
namespace ShipGlyph {

void draw(Framebuffer& fb, int cx, int cy, uint16_t color,
          float heading_deg = 0.0f, float scale = 1.0f);

}  // namespace ShipGlyph

#endif  // SHIP_GLYPH_H
