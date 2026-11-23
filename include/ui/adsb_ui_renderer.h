#ifndef ADSB_UI_RENDERER_H
#define ADSB_UI_RENDERER_H

#include "ui/base_ui_renderer.h"
#include "adsb/adsb_message.h"
#include <vector>

/**
 * ADSBUIRenderer - UI renderer for ADSB page
 *
 * Renders aircraft list in table format with fixed-width columns
 * Layout optimized for 320x170 display
 */
class ADSBUIRenderer : public BaseUIRenderer {
public:
    explicit ADSBUIRenderer(Framebuffer& fb);
    ~ADSBUIRenderer();

    /**
     * Render aircraft list in table format
     * @param aircraft Vector of aircraft to display
     * @param selected_index Index of selected aircraft (-1 for none)
     * @param scroll_offset Scroll offset for list
     */
    void renderAircraftList(const std::vector<AircraftState>& aircraft,
                           int selected_index,
                           int scroll_offset);

    /**
     * Render top status bar
     * @param aircraft_count Number of aircraft
     * @param message_count Total messages decoded
     * @param message_rate Messages per second
     * @param gain Current gain value
     */
    void renderStatusBar(int aircraft_count,
                        uint32_t message_count,
                        float message_rate);

    /**
     * Render bottom statistics bar
     * @param total_messages Total messages
     * @param crc_errors CRC error count
     * @param max_range Maximum range (km)
     */
    void renderStatistics(uint32_t total_messages,
                         uint32_t crc_errors,
                         float max_range);

    /**
     * Render column headers
     */
    void renderColumnHeaders();

    // H overlay: key reference and settings summary
    void renderKeyHelp();
    void renderSettingsOverview(double lat, double lon, bool pos_set, int selected,
                                const char* source);

    /**
     * Render detail view for selected aircraft
     * @param aircraft Aircraft to display
     * @param scroll_offset Vertical scroll offset in pixels
     * @param aircraft_count Total aircraft count for status bar
     * @param message_count Total messages for status bar
     * @param message_rate Messages per second for status bar
     * @param gain Current gain value for status bar
     */
    void renderAircraftDetail(const AircraftState& aircraft,
                              int scroll_offset,
                              int aircraft_count,
                              uint32_t message_count,
                              float message_rate,
                              float gain);

    /**
     * Full-screen altitude-vs-distance chart (vertical profile).
     * X = range km, Y = altitude (GND .. FL400). Selected craft is amber.
     */
    void renderVerticalProfile(const std::vector<AircraftState>& aircraft,
                               int selected_index,
                               float max_range_km);

    /**
     * Render text at specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @param text Text to render
     * @param color Text color
     * @param bold Use bold font (ignored, for compatibility)
     */
    void renderText(int x, int y, const char* text, uint16_t color, bool bold = false);

    // Rounded translucent panel used by the H overlay.
    void drawRoundedPanel(int x, int y, int w, int h, int radius,
                          uint16_t color, float alpha);

    // Use 8x16 font for the ADSB page (override BaseUIRenderer defaults)
    void drawChar(int x, int y, char c, uint16_t color) override;
    void drawText(int x, int y, const char* text, uint16_t color) override;
    void drawTextBg(int x, int y, const char* text, uint16_t fg, uint16_t bg) override;
    int getTextWidth(const char* text) const override;

private:
    // Layout constants (320x170)
    static const int STATUS_BAR_Y = 0;
    static const int STATUS_BAR_H = 16;
    static const int HEADER_Y = 16;
    static const int HEADER_H = 14;
    static const int LIST_Y = 30;
    static const int LIST_H = 122;           // 30 to 152
    static const int STATS_BAR_Y = 152;
    static const int STATS_BAR_H = 18;
    static const int ROW_HEIGHT = 14;        // mono line + padding
    static const int MAX_VISIBLE_ROWS = 8;   // 122 / 14 ≈ 8

    // Column positions (approximate mono advances)
    static const int COL_ICAO_X = 2;
    static const int COL_ICAO_W = 48;
    static const int COL_CALL_X = 52;
    static const int COL_CALL_W = 64;
    static const int COL_ALT_X = 120;
    static const int COL_ALT_W = 40;
    static const int COL_SPD_X = 164;
    static const int COL_SPD_W = 28;
    static const int COL_HDG_X = 196;
    static const int COL_HDG_W = 28;
    static const int COL_DIST_X = 228;
    static const int COL_DIST_W = 40;
    static const int COL_SEEN_X = 272;
    static const int COL_SEEN_W = 44;

    /**
     * Format time since last seen
     * @param seconds Seconds since last message
     * @param buffer Output buffer (must be at least 5 chars)
     */
    void formatTimeSince(time_t seconds, char* buffer);

    /**
     * Format distance with unit
     * @param distance Distance in km
     * @param buffer Output buffer (must be at least 6 chars)
     */
    void formatDistance(float distance, char* buffer);

    // Vertical profile helpers
    void drawDashedHLine(int x, int y, int length, uint16_t color,
                         int dash = 3, int gap = 3);
    void drawProfileTrendArrow(int x, int y, int direction, uint16_t color);

    // PFD rendering helpers
    void renderPFD(const AircraftState& aircraft, int x, int y, int w, int h);
    void drawArtificialHorizon(int cx, int cy, int radius, float pitch_angle);
    void drawPitchLadder(int cx, int cy, int width, int height, float current_pitch);
    void drawBankAngleIndicator(int cx, int cy, int radius);
    void drawAltitudeTape(int x, int y, int w, int h, int altitude);
    void drawSpeedTape(int x, int y, int w, int h, int speed);
    void drawHeadingIndicator(int x, int y, int w, int h, int heading);
    void drawVerticalSpeedIndicator(int x, int y, int w, int h, int vs);
    void drawCircleApprox(int cx, int cy, int radius, uint16_t color);
    float calculatePitchAngle(const AircraftState& aircraft);

    // 6x8 small font drawing (for labels on PFD instruments)
    void drawSmallText(int x, int y, const char* text, uint16_t color);
};

#endif // ADSB_UI_RENDERER_H
