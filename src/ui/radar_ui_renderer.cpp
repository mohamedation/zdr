#include "ui/radar_ui_renderer.h"
#ifndef ZDR_UI_PREVIEW
#include "adsb/map_background.h"
#endif
#include "ui/aircraft_glyph.h"
#include "ui/ui_text.h"
#include "font.h"
#include <cstring>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <algorithm>

RadarUIRenderer::RadarUIRenderer(Framebuffer& fb) : BaseUIRenderer(fb) {
}

RadarUIRenderer::~RadarUIRenderer() {
}

void RadarUIRenderer::drawMapBackground(MapBackground& map, int cx, int cy,
                                        int radius, float max_range,
                                        float heading_deg) {
#ifdef ZDR_UI_PREVIEW
    (void)map;
    (void)cx;
    (void)cy;
    (void)radius;
    (void)max_range;
    (void)heading_deg;
#else
    map.draw(fb_, cx, cy, radius, max_range, heading_deg);
#endif
}

void RadarUIRenderer::renderRadarDisplay(const std::vector<AircraftState>& aircraft,
                                         float sweep_angle,
                                         float max_range,
                                         double observer_lat,
                                         double observer_lon,
                                         int selected_index,
                                         int scroll_offset,
                                         bool blink_state,
                                         float heading_deg,
                                         MapBackground* map) {
    (void)observer_lat;
    (void)observer_lon;
    (void)sweep_angle;  // Cosmetic beam removed; arg kept for API stability.

    // Flat black content area (same canvas as profile / radar-only).
    fb_.fillRect(0, RADAR_Y, 320, RADAR_H, COLOR_BLACK);

#ifndef ZDR_UI_PREVIEW
    // Basemap under the scope (list stays black).
    if (map != nullptr && map->hasImage()) {
        drawMapBackground(*map, RADAR_CENTER_X, RADAR_CENTER_Y, RADAR_RADIUS,
                          max_range, heading_deg);
    }
#else
    (void)map;
#endif

    // Vertical separator between list and radar
    drawVLine(LIST_W, RADAR_Y, RADAR_H, COLOR_RADAR_GRID);

    renderAircraftList(aircraft, selected_index, scroll_offset);

    drawRangeRings(RADAR_CENTER_X, RADAR_CENTER_Y, RADAR_RADIUS,
                   NUM_RANGE_RINGS, max_range, heading_deg);
    drawCompassMarkings(RADAR_CENTER_X, RADAR_CENTER_Y, RADAR_RADIUS,
                        heading_deg, LIST_W + 1);
    drawRingLabels(RADAR_CENTER_X, RADAR_CENTER_Y, RADAR_RADIUS,
                   NUM_RANGE_RINGS, max_range);

    for (size_t i = 0; i < aircraft.size(); i++) {
        const auto& ac = aircraft[i];
        if (ac.position_valid && ac.distance > 0) {
            const bool is_selected = (i == (size_t)selected_index);
            if (is_selected && !blink_state) {
                continue;
            }
            drawAircraftBlip(RADAR_CENTER_X, RADAR_CENTER_Y, RADAR_RADIUS,
                             ac.distance, ac.bearing, ac.heading,
                             ac.velocity_valid, max_range, is_selected,
                             heading_deg, COLOR_RADAR_SELECTED);
        }
    }

    // Full-width bottom chrome — Caption callsign + nav (profile style).
    drawHLine(2, BOTTOM_Y, 316, COLOR_RADAR_GRID);

    char callsign[9] = {0};
    char info[24] = {0};
    if (selected_index >= 0 && selected_index < (int)aircraft.size()) {
        const auto& ac = aircraft[selected_index];
        int n = 0;
        for (; n < 8 && ac.callsign[n] && ac.callsign[n] != ' '; ++n) {
            callsign[n] = ac.callsign[n];
        }
        callsign[n] = '\0';
        if (n == 0) {
            snprintf(callsign, sizeof(callsign), "%06X", ac.icao_addr);
        }
        if (ac.distance > 0.0f) {
            if (ac.altitude > 0) {
                snprintf(info, sizeof(info), "%.0fkm %dft", ac.distance,
                         ac.altitude);
            } else {
                snprintf(info, sizeof(info), "%.0fkm", ac.distance);
            }
        }
    } else {
        snprintf(callsign, sizeof(callsign), "---");
    }

    const uint16_t callsign_color =
        (selected_index >= 0) ? COLOR_RADAR_SELECTED : COLOR_RADAR_TEXT;
    uiText().draw(fb_, 6, BOTTOM_Y + 3, callsign, TextStyle::Caption,
                  callsign_color);
    if (info[0] != '\0') {
        const int call_w = uiText().width(callsign, TextStyle::Caption);
        uiText().draw(fb_, 6 + call_w + 8, BOTTOM_Y + 3, info,
                      TextStyle::Caption, COLOR_RADAR_TEXT);
    }
    const char* hints = "Up/Dn Change  M Menu";
    const int hints_w = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hints_w, BOTTOM_Y + 3, hints,
                  TextStyle::Caption, COLOR_RADAR_TEXT);
}

void RadarUIRenderer::renderRadarScope(const std::vector<AircraftState>& aircraft,
                                       float sweep_angle,
                                       float max_range,
                                       int selected_index,
                                       float heading_deg,
                                       MapBackground* map) {
    (void)sweep_angle;  // Cosmetic beam removed; arg kept for API stability.

    // Same flat black canvas as vertical profile (no bar backgrounds).
    fb_.fillRect(0, SCOPE_RADAR_Y, 320, SCOPE_RADAR_H, COLOR_BLACK);

#ifndef ZDR_UI_PREVIEW
    if (map != nullptr && map->hasImage()) {
        drawMapBackground(*map, SCOPE_CENTER_X, SCOPE_CENTER_Y, SCOPE_RADIUS,
                          max_range, heading_deg);
    }
#else
    (void)map;
#endif

    drawRangeRings(SCOPE_CENTER_X, SCOPE_CENTER_Y, SCOPE_RADIUS,
                   NUM_RANGE_RINGS, max_range, heading_deg);
    drawCompassMarkings(SCOPE_CENTER_X, SCOPE_CENTER_Y, SCOPE_RADIUS,
                        heading_deg, 1);
    drawRingLabels(SCOPE_CENTER_X, SCOPE_CENTER_Y, SCOPE_RADIUS,
                   NUM_RANGE_RINGS, max_range);

    for (size_t i = 0; i < aircraft.size(); i++) {
        const auto& ac = aircraft[i];
        if (!ac.position_valid || ac.distance <= 0.0f) continue;
        if (ac.distance > max_range) continue;

        const bool is_selected = (i == (size_t)selected_index);
        drawAircraftBlip(SCOPE_CENTER_X, SCOPE_CENTER_Y, SCOPE_RADIUS,
                         ac.distance, ac.bearing,
                         ac.heading, ac.velocity_valid,
                         max_range, is_selected, heading_deg,
                         COLOR_RADAR_SELECTED);
    }

    // Bottom chrome — text only on the shared black canvas (profile style).
    drawHLine(2, SCOPE_BOTTOM_Y, 316, COLOR_RADAR_GRID);

    char callsign[9] = {0};
    char info[24] = {0};
    if (selected_index >= 0 && selected_index < (int)aircraft.size()) {
        const auto& ac = aircraft[selected_index];
        int n = 0;
        for (; n < 8 && ac.callsign[n] && ac.callsign[n] != ' '; ++n) {
            callsign[n] = ac.callsign[n];
        }
        callsign[n] = '\0';
        if (n == 0) {
            snprintf(callsign, sizeof(callsign), "%06X", ac.icao_addr);
        }
        if (ac.position_valid && ac.distance > 0.0f) {
            if (ac.altitude > 0) {
                snprintf(info, sizeof(info), "%.0fkm %dft", ac.distance,
                         ac.altitude);
            } else {
                snprintf(info, sizeof(info), "%.0fkm", ac.distance);
            }
        }
    } else {
        snprintf(callsign, sizeof(callsign), "---");
    }

    const uint16_t callsign_color =
        (selected_index >= 0) ? COLOR_RADAR_SELECTED : COLOR_RADAR_TEXT;
    uiText().draw(fb_, 6, SCOPE_BOTTOM_Y + 3, callsign, TextStyle::Caption,
                  callsign_color);

    if (info[0] != '\0') {
        const int call_w = uiText().width(callsign, TextStyle::Caption);
        uiText().draw(fb_, 6 + call_w + 8, SCOPE_BOTTOM_Y + 3, info,
                      TextStyle::Caption, COLOR_RADAR_TEXT);
    }

    const char* hints = "Up/Dn Change  M Menu";
    const int hints_w = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hints_w, SCOPE_BOTTOM_Y + 3, hints,
                  TextStyle::Caption, COLOR_RADAR_TEXT);
}

void RadarUIRenderer::renderScopeStatusBar(int in_view_count, int total_count,
                                           float max_range,
                                           bool heading_valid, float heading_deg,
                                           bool heading_up) {
    // Top chrome matches vertical profile: Caption on the shared canvas,
    // no separate bar fill.
    char title[32];
    snprintf(title, sizeof(title), "RADAR  %.0fkm", max_range);
    uiText().draw(fb_, 6, SCOPE_HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_RADAR_TEXT);

    char trk[24];
    if (heading_valid) {
        snprintf(trk, sizeof(trk), "Trk:%d/%d  HDG%03.0f", in_view_count,
                 total_count,
                 heading_deg < 0.0f ? heading_deg + 360.0f : heading_deg);
        (void)heading_up;
    } else {
        snprintf(trk, sizeof(trk), "Trk: %d/%d", in_view_count, total_count);
    }
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    const uint16_t trk_color =
        (heading_valid && heading_up) ? COLOR_RADAR_SELECTED : COLOR_RADAR_TEXT;
    uiText().draw(fb_, 320 - 8 - trk_w, SCOPE_HEADER_Y + 2, trk,
                  TextStyle::Caption, trk_color);
}

void RadarUIRenderer::renderStatusBar(int aircraft_count, float max_range,
                                      bool heading_valid, float heading_deg,
                                      bool heading_up) {
    // Caption chrome on the shared black canvas — no bar fill.
    char title[32];
    snprintf(title, sizeof(title), "RADAR  %.0fkm", max_range);
    uiText().draw(fb_, 6, HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_RADAR_TEXT);

    char trk[28];
    if (heading_valid) {
        snprintf(trk, sizeof(trk), "Trk:%d  HDG%03.0f", aircraft_count,
                 heading_deg < 0.0f ? heading_deg + 360.0f : heading_deg);
    } else {
        snprintf(trk, sizeof(trk), "Trk: %d", aircraft_count);
    }
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    const uint16_t trk_color =
        (heading_valid && heading_up) ? COLOR_RADAR_SELECTED : COLOR_RADAR_TEXT;
    uiText().draw(fb_, 320 - 8 - trk_w, HEADER_Y + 2, trk, TextStyle::Caption,
                  trk_color);
}

void RadarUIRenderer::renderAircraftList(const std::vector<AircraftState>& aircraft,
                                         int selected_index,
                                         int scroll_offset) {
    // List sits on the shared black canvas (no panel fill).
    uiText().draw(fb_, LIST_X + 2, LIST_Y + 2, "ICAO", TextStyle::Caption,
                  COLOR_RADAR_TEXT);
    uiText().draw(fb_, LIST_X + 52, LIST_Y + 2, "ALT", TextStyle::Caption,
                  COLOR_RADAR_TEXT);
    uiText().draw(fb_, LIST_X + 88, LIST_Y + 2, "DIST", TextStyle::Caption,
                  COLOR_RADAR_TEXT);
    drawHLine(LIST_X, LIST_Y + ROW_HEIGHT - 1, LIST_W, COLOR_RADAR_GRID);

    const int start_idx = scroll_offset;
    const int end_idx =
        std::min((int)aircraft.size(), scroll_offset + MAX_VISIBLE_ROWS - 1);

    for (int i = start_idx; i < end_idx; i++) {
        const auto& ac = aircraft[i];
        const int row_y = LIST_Y + ROW_HEIGHT + (i - scroll_offset) * ROW_HEIGHT;
        const bool is_selected = (i == selected_index);
        if (is_selected) {
            fb_.fillRect(LIST_X, row_y, LIST_W, ROW_HEIGHT, COLOR_RADAR_SELECTED);
        }

        const uint16_t text_color =
            is_selected ? COLOR_BLACK : COLOR_RADAR_TEXT;
        char buffer[32];

        snprintf(buffer, sizeof(buffer), "%06X", ac.icao_addr);
        uiText().draw(fb_, LIST_X + 2, row_y + 2, buffer, TextStyle::Caption,
                      text_color);

        if (ac.position_valid && ac.altitude > 0) {
            if (ac.altitude >= 10000) {
                snprintf(buffer, sizeof(buffer), "%dk", ac.altitude / 1000);
            } else {
                snprintf(buffer, sizeof(buffer), "%.1fk", ac.altitude / 1000.0f);
            }
        } else {
            snprintf(buffer, sizeof(buffer), "-");
        }
        const int alt_w = uiText().width(buffer, TextStyle::Caption);
        uiText().draw(fb_, LIST_X + 82 - alt_w, row_y + 2, buffer,
                      TextStyle::Caption, text_color);

        if (ac.distance > 0) {
            if (ac.distance < 10.0f) {
                snprintf(buffer, sizeof(buffer), "%.1fk", ac.distance);
            } else {
                snprintf(buffer, sizeof(buffer), "%.0fk", ac.distance);
            }
        } else {
            snprintf(buffer, sizeof(buffer), "-");
        }
        const int dist_w = uiText().width(buffer, TextStyle::Caption);
        uiText().draw(fb_, LIST_X + 122 - dist_w, row_y + 2, buffer,
                      TextStyle::Caption, text_color);
    }

    // Draw scroll indicator if needed
    if (aircraft.size() > (size_t)MAX_VISIBLE_ROWS - 1) {
        int total_rows = aircraft.size();
        int visible_rows = MAX_VISIBLE_ROWS - 1;

        // Draw scroll bar on right edge
        int scrollbar_h = (LIST_H - ROW_HEIGHT) * visible_rows / total_rows;
        int scrollbar_y = LIST_Y + ROW_HEIGHT + (LIST_H - ROW_HEIGHT - scrollbar_h) * scroll_offset / (total_rows - visible_rows);

        for (int y = scrollbar_y; y < scrollbar_y + scrollbar_h; y++) {
            fb_.drawPixel(LIST_X + LIST_W - 2, y, COLOR_RADAR_TEXT);
        }
    }
}

void RadarUIRenderer::renderText(int x, int y, const char* text, uint16_t color, bool bold) {
    drawText(x, y, text, color);
}

void RadarUIRenderer::drawRangeRings(int cx, int cy, int radius, int num_rings,
                                     float max_range, float rotation_deg) {
    (void)max_range;
    // Inner rings stay dim; outer ring is the bright scale reference.
    for (int i = 1; i <= num_rings; i++) {
        int ring_radius = (radius * i) / num_rings;
        if (i < num_rings) {
            drawDashedCircleApprox(cx, cy, ring_radius, COLOR_RADAR_GRID);
        } else {
            drawCircleApprox(cx, cy, ring_radius, COLOR_RADAR_SWEEP);
        }
    }

    // Tick marks on the outer ring follow the compass rose.
    for (int angle = 0; angle < 360; angle += 15) {
        float rad = (angle - 90.0f - rotation_deg) * M_PI / 180.0f;

        int tick_length = (angle % 30 == 0) ? 4 : 2;
        const uint16_t tick_color =
            (angle % 90 == 0) ? COLOR_RADAR_SWEEP : COLOR_RADAR_GRID;

        int x1 = cx + (int)((radius - tick_length) * cos(rad));
        int y1 = cy + (int)((radius - tick_length) * sin(rad));
        int x2 = cx + (int)(radius * cos(rad));
        int y2 = cy + (int)(radius * sin(rad));

        fb_.drawLine(x1, y1, x2, y2, tick_color);
    }
}

void RadarUIRenderer::drawScaledText(int x, int y, const char* text,
                                     uint16_t color, int scale) {
    if (scale < 1) scale = 1;
    int cursor_x = x;
    for (const char* p = text; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c > 0x7E) c = '?';
        const uint8_t* glyph = font6x8[c - 0x20];
        for (int row = 0; row < FONT_6X8_HEIGHT; ++row) {
            const uint8_t line = glyph[row];
            for (int col = 0; col < FONT_6X8_WIDTH; ++col) {
                if (line & (0x80 >> col)) {
                    if (scale == 1) {
                        fb_.drawPixel(cursor_x + col, y + row, color);
                    } else {
                        fb_.fillRect(cursor_x + col * scale, y + row * scale,
                                     scale, scale, color);
                    }
                }
            }
        }
        cursor_x += (FONT_6X8_WIDTH + 1) * scale;
    }
}

void RadarUIRenderer::drawRingLabels(int cx, int cy, int radius, int num_rings,
                                     float max_range) {
    // Range labels sit on the south-west diagonal (225 deg), inside the ring,
    // away from the S cardinal label. Drawn after the sweep so they stay
    // readable while it passes.
    const float diag = 0.7071f;
    for (int i = 1; i <= num_rings; i++) {
        const int ring_radius = (radius * i) / num_rings;
        const float ring_range = (max_range * i) / num_rings;

        char label[8];
        snprintf(label, sizeof(label), "%.0f", ring_range);

        const int label_x = cx - (int)(ring_radius * diag) - 8;
        const int label_y = cy + (int)(ring_radius * diag) - 4;

        if (label_y > 0 && label_y < 170 - 9) {
            drawText(label_x, label_y, label, COLOR_RADAR_TEXT);
        }
    }
}

void RadarUIRenderer::drawCompassMarkings(int cx, int cy, int radius,
                                          float rotation_deg, int min_x) {
    // Cardinal labels turn with the compass rose. N stays brightest.
    static const char* labels[4] = {"N", "E", "S", "W"};
    const float label_radius = (float)radius + 7.0f;
    for (int i = 0; i < 4; ++i) {
        const float screen_angle =
            ((float)(i * 90) - rotation_deg) * (float)M_PI / 180.0f;
        int x = cx + (int)lroundf(label_radius * sinf(screen_angle)) - 3;
        int y = cy - (int)lroundf(label_radius * cosf(screen_angle)) - 4;
        x = std::max(min_x, std::min(320 - 8, x));
        y = std::max(1, std::min(170 - 9, y));
        const uint16_t color =
            (i == 0) ? COLOR_RADAR_SWEEP : COLOR_RADAR_GRID;
        drawText(x, y, labels[i], color);
    }

    // Cardinal ticks on the outer ring.
    for (int angle = 0; angle < 360; angle += 90) {
        float rad = (angle - 90.0f - rotation_deg) * M_PI / 180.0f;
        int x1 = cx + (int)((radius - 3) * cos(rad));
        int y1 = cy + (int)((radius - 3) * sin(rad));
        int x2 = cx + (int)(radius * cos(rad));
        int y2 = cy + (int)(radius * sin(rad));
        const uint16_t color =
            (angle == 0) ? COLOR_RADAR_SWEEP : COLOR_RADAR_GRID;
        fb_.drawLine(x1, y1, x2, y2, color);
    }
}

void RadarUIRenderer::drawSweep(int cx, int cy, int radius, float angle) {
    // A single translucent wedge instead of a stack of discrete lines:
    // every pixel inside the circle is blended by its angular distance
    // behind the sweep, so the tail fades smoothly with no banding.
    constexpr float kTrailDeg = 50.0f;   // visible tail length
    constexpr float kInvDeg = 57.29578f; // 180/pi

    const int x0 = std::max(0, cx - radius);
    const int x1 = std::min(fb_.getWidth() - 1, cx + radius);
    const int y0 = std::max(0, cy - radius);
    const int y1 = std::min(fb_.getHeight() - 1, cy + radius);
    const int r2_max = radius * radius;

    for (int y = y0; y <= y1; ++y) {
        const int dy = cy - y;                 // screen y grows downwards
        const int dy2 = dy * dy;
        for (int x = x0; x <= x1; ++x) {
            const int dx = x - cx;
            if (dx * dx + dy2 > r2_max) continue;

            // Convert to the radar convention: 0 deg = north, clockwise.
            // math angle is counter-clockwise from +x, so radar = 90 - math.
            float pix_angle = 90.0f - atan2f((float)dy, (float)dx) * kInvDeg;
            if (pix_angle < 0.0f) pix_angle += 360.0f;

            float delta = fmodf(angle - pix_angle, 360.0f);
            if (delta < 0.0f) delta += 360.0f;  // 0..360 behind the sweep
            if (delta > kTrailDeg) continue;

            const float t = 1.0f - delta / kTrailDeg;
            float alpha = t * t * 0.80f;        // quadratic fade
            if (delta < 1.0f) alpha = 0.95f;    // bright leading edge
            fb_.blendPixel(x, y, COLOR_RADAR_SWEEP, alpha);
        }
    }
}

void RadarUIRenderer::drawAircraftBlip(int cx, int cy, int radius,
                                       float distance, float bearing,
                                       float heading, bool velocity_valid,
                                       float max_range, bool selected,
                                       float rotation_deg,
                                       uint16_t selected_color) {
    // Convert polar to Cartesian. A heading-up scope rotates both the
    // position and the aircraft glyph by the same angle.
    int blip_x, blip_y;
    polarToCartesian(distance, bearing - rotation_deg, max_range,
                     cx, cy, radius, &blip_x, &blip_y);

    const uint16_t color = selected ? selected_color : COLOR_RADAR_BLIP;
    const float h = (velocity_valid ? heading : 0.0f) - rotation_deg;
    // Selection is color-only (no ring) — same language as vertical profile.
    AircraftGlyph::draw(fb_, blip_x, blip_y, color, h, 1.0f);
}

void RadarUIRenderer::drawCircleApprox(int cx, int cy, int radius, uint16_t color) {
    // Bresenham's circle algorithm (outline only)
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    while (x <= y) {
        // Draw 8 octants
        fb_.drawPixel(cx + x, cy + y, color);
        fb_.drawPixel(cx - x, cy + y, color);
        fb_.drawPixel(cx + x, cy - y, color);
        fb_.drawPixel(cx - x, cy - y, color);
        fb_.drawPixel(cx + y, cy + x, color);
        fb_.drawPixel(cx - y, cy + x, color);
        fb_.drawPixel(cx + y, cy - x, color);
        fb_.drawPixel(cx - y, cy - x, color);

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void RadarUIRenderer::drawDashedCircleApprox(int cx, int cy, int radius, uint16_t color) {
    // Dashed outline via parametric walk (~1 pixel of arc per step).
    // Dash pattern: 3 pixels on, 3 pixels off.
    const int dash_pixels = 3;
    const int gap_pixels = 3;
    if (radius <= 0) return;

    float step = 1.0f / (float)radius;
    if (step < 0.01f) step = 0.01f;

    int count = 0;
    bool drawing = true;

    for (float angle = 0.0f; angle < 2.0f * M_PI; angle += step) {
        if (drawing) {
            int x = cx + (int)(radius * cos(angle));
            int y = cy + (int)(radius * sin(angle));
            fb_.drawPixel(x, y, color);
        }

        count++;
        if (count >= (drawing ? dash_pixels : gap_pixels)) {
            drawing = !drawing;
            count = 0;
        }
    }
}

uint16_t RadarUIRenderer::blendColor(uint16_t color1, uint16_t color2, float alpha) {
    // Extract RGB components from RGB565
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;

    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;

    // Blend components
    uint8_t r = (uint8_t)(r1 * alpha + r2 * (1.0f - alpha));
    uint8_t g = (uint8_t)(g1 * alpha + g2 * (1.0f - alpha));
    uint8_t b = (uint8_t)(b1 * alpha + b2 * (1.0f - alpha));

    // Combine back to RGB565
    return (r << 11) | (g << 5) | b;
}

void RadarUIRenderer::polarToCartesian(float distance, float bearing, float max_range,
                                       int cx, int cy, int radius,
                                       int* out_x, int* out_y) {
    float normalized_dist = (max_range > 0.0f)
                                ? (distance / max_range) * static_cast<float>(radius)
                                : 0.0f;
    if (normalized_dist > static_cast<float>(radius)) {
        normalized_dist = static_cast<float>(radius);
    }

    // Convert bearing to radians (0° = North, clockwise)
    // Screen coordinates: 0° = up, 90° = right
    float angle_rad = (bearing - 90.0f) * M_PI / 180.0f;

    *out_x = cx + (int)(normalized_dist * cos(angle_rad));
    *out_y = cy + (int)(normalized_dist * sin(angle_rad));
}
