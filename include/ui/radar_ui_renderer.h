#ifndef RADAR_UI_RENDERER_H
#define RADAR_UI_RENDERER_H

#include "ui/base_ui_renderer.h"
#include "ui/ttf_text.h"
#include "adsb/adsb_message.h"
#include <vector>
#include <cmath>

class MapBackground;

/**
 * RadarUIRenderer - UI renderer for radar page
 *
 * Renders professional radar display with:
 * - Circular radar scope with range rings
 * - Rotating sweep line with fading trail
 * - Aircraft blips in polar coordinates
 * - Compass markings
 * Layout optimized for 320x170 display
 */
class RadarUIRenderer : public BaseUIRenderer {
public:
    explicit RadarUIRenderer(Framebuffer& fb);
    ~RadarUIRenderer();

    /**
     * Render complete radar display with left-right split layout
     * @param aircraft Vector of aircraft to display
     * @param sweep_angle Current sweep angle (0-360°)
     * @param max_range Maximum range in km
     * @param observer_lat Observer latitude
     * @param observer_lon Observer longitude
     * @param selected_index Index of selected aircraft (-1 for none)
     * @param scroll_offset Scroll offset for aircraft list
     * @param blink_state Unused (kept for API stability; selection is solid)
     * @param heading_deg Heading placed at the top of the scope (0 = north up)
     */
    void renderRadarDisplay(const std::vector<AircraftState>& aircraft,
                           float sweep_angle,
                           float max_range,
                           double observer_lat,
                           double observer_lon,
                           int selected_index,
                           int scroll_offset,
                           bool blink_state,
                           float heading_deg = 0.0f,
                           MapBackground* map = nullptr);

    /**
     * Full-screen radar (no list). Map fills the display; aircraft out to
     * the screen edges stay visible when zoomed past the range ring.
     * Bottom bar: callsign on the left; range / distance on the right.
     */
    void renderRadarScope(const std::vector<AircraftState>& aircraft,
                          float sweep_angle,
                          float max_range,
                          int selected_index,
                          float heading_deg = 0.0f,
                          MapBackground* map = nullptr);

    /** Draw optional Web-Mercator basemap under the scope (before rings). */
    void drawMapBackground(MapBackground& map, int cx, int cy, int radius,
                           float max_range, float heading_deg = 0.0f,
                           bool fill_frame = false);

    /** Max distance (km) that still plots on the full-screen scope. */
    static float scopePlotLimitKm(float max_range);

    /**
     * Render top status bar
     * @param aircraft_count Number of aircraft
     * @param max_range Maximum range (km)
     * @param heading_valid Whether the compass has a heading
     * @param heading_deg Compass heading in degrees
     * @param heading_up Whether the scope is heading-up (otherwise north-up)
     */
    void renderStatusBar(int aircraft_count, float max_range,
                         bool heading_valid = false,
                         float heading_deg = 0.0f,
                         bool heading_up = false);

    /** Top status bar for the full-screen radar scope. */
    void renderScopeStatusBar(int in_view_count, int total_count,
                              float max_range,
                              bool heading_valid = false,
                              float heading_deg = 0.0f,
                              bool heading_up = false);

    /**
     * Render aircraft list in left panel
     * @param aircraft Vector of aircraft to display
     * @param selected_index Index of selected aircraft
     * @param scroll_offset Scroll offset for list
     */
    void renderAircraftList(const std::vector<AircraftState>& aircraft,
                           int selected_index,
                           int scroll_offset);

    /**
     * Render text at specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @param text Text to render
     * @param color Text color
     * @param bold Use bold font (ignored, for compatibility)
     */
    void renderText(int x, int y, const char* text, uint16_t color, bool bold = false);

private:
    // Layout constants (320x170) — chrome matches profile / radar-only scope
    static const int HEADER_Y = 2;
    static const int BOTTOM_Y = 154;
    static const int BOTTOM_H = 16;

    // Left panel - Aircraft list
    static const int LIST_X = 0;
    static const int LIST_Y = 22;
    static const int LIST_W = 126;
    static const int LIST_H = 132;           // 22 .. 154
    static const int ROW_HEIGHT = 14;
    static const int MAX_VISIBLE_ROWS = 9;   // header + 8 rows

    // Right panel - Radar display
    static const int RADAR_X = 126;
    static const int RADAR_Y = 22;
    static const int RADAR_W = 194;
    static const int RADAR_H = 132;
    static const int RADAR_CENTER_X = 223;  // 126 + 194/2
    static const int RADAR_CENTER_Y = 88;
    static const int RADAR_RADIUS = 58;
    static const int NUM_RANGE_RINGS = 3;
    static const int NUM_TRAIL_LINES = 4;

    // Full-screen radar scope — chrome matches vertical profile layout
    static const int SCOPE_HEADER_Y = 2;
    static const int SCOPE_BOTTOM_Y = 154;
    static const int SCOPE_BOTTOM_H = 16;
    static const int SCOPE_RADAR_Y = 22;
    static const int SCOPE_RADAR_H = 132;  // 22 .. 154
    static const int SCOPE_CENTER_X = 160;
    static const int SCOPE_CENTER_Y = 88;
    static const int SCOPE_RADIUS = 58;

    /**
     * Draw range rings
     * @param cx Center X
     * @param cy Center Y
     * @param radius Outer radius
     * @param num_rings Number of rings
     * @param max_range Maximum range for labels
     * @param rotation_deg Rotation of the compass rose (heading at the top)
     */
    void drawRangeRings(int cx, int cy, int radius, int num_rings,
                        float max_range, float rotation_deg = 0.0f);

    /**
     * Draw compass markings (N/E/S/W)
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radar radius
     * @param rotation_deg Rotation of the compass rose (heading at the top)
     */
    void drawCompassMarkings(int cx, int cy, int radius,
                             float rotation_deg = 0.0f,
                             int min_x = LIST_W + 1);
    void drawRingLabels(int cx, int cy, int radius, int num_rings, float max_range);
    void drawScaledText(int x, int y, const char* text, uint16_t color, int scale);

    // Anti-aliased font for the selected callsign (loaded lazily).
    mutable TtfText callsign_font_;
    mutable bool callsign_font_loaded_ = false;
    mutable TtfText ui_font_;
    mutable bool ui_font_loaded_ = false;

    /**
     * Draw sweep line at current angle
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radar radius
     * @param angle Sweep angle (0-360°)
     */
    void drawSweep(int cx, int cy, int radius, float angle);

    /**
     * Draw fading trail behind sweep line
     * @param cx Center X
     * @param cy Center Y
     * @param radius Radar radius
     * @param angle Current sweep angle
     */

    /**
     * Draw aircraft blip with heading vector
     * @param cx Center X
     * @param cy Center Y
     * @param distance Distance in km
     * @param bearing Bearing in degrees
     * @param heading Heading in degrees (0=N, clockwise)
     * @param velocity_valid Whether heading is meaningful
     * @param max_range Maximum range for scaling
     * @param selected Whether this aircraft is selected
     * @param rotation_deg Rotation of the compass rose (heading at the top)
     */
    void drawAircraftBlip(int cx, int cy, int radius,
                         float distance, float bearing,
                         float heading, bool velocity_valid,
                         float max_range, bool selected,
                         float rotation_deg = 0.0f,
                         uint16_t selected_color = COLOR_RADAR_SELECTED);

    /**
     * Draw circle outline (for range rings)
     * @param cx Center X
     * @param cy Center Y
     * @param radius Circle radius
     * @param color Line color
     */
    void drawCircleApprox(int cx, int cy, int radius, uint16_t color);

    /**
     * Draw dashed circle outline (for inner range rings)
     * @param cx Center X
     * @param cy Center Y
     * @param radius Circle radius
     * @param color Line color
     */
    void drawDashedCircleApprox(int cx, int cy, int radius, uint16_t color);

    /**
     * Blend two colors with alpha
     * @param color1 First color (RGB565)
     * @param color2 Second color (RGB565)
     * @param alpha Blend factor (0.0 = color2, 1.0 = color1)
     * @return Blended color (RGB565)
     */
    uint16_t blendColor(uint16_t color1, uint16_t color2, float alpha);

    /**
     * Convert polar coordinates to Cartesian screen coordinates
     * @param distance Distance in km
     * @param bearing Bearing in degrees (0° = North, clockwise)
     * @param max_range Maximum range for scaling
     * @param out_x Output X coordinate
     * @param out_y Output Y coordinate
     */
    void polarToCartesian(float distance, float bearing, float max_range,
                         int cx, int cy, int radius,
                         int* out_x, int* out_y);
};

#endif // RADAR_UI_RENDERER_H
