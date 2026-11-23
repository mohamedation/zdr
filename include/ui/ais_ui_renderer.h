#ifndef AIS_UI_RENDERER_H
#define AIS_UI_RENDERER_H

#include "ais/ais_store.h"
#include "ui/base_ui_renderer.h"

#include <cstdint>
#include <vector>

/**
 * AisUIRenderer - AIS list / plot / dossier matching ADSB / radar chrome
 * (UiText Caption + Mono, flat black canvas, amber selection only).
 */
class MapBackground;

class AisUIRenderer : public BaseUIRenderer {
public:
    explicit AisUIRenderer(Framebuffer& fb);

    void renderList(const std::vector<ais::VesselState>& vessels,
                    int selected_index, int scroll_offset,
                    uint64_t total_messages, float messages_per_second,
                    int gain_tenths, bool agc);

    void renderConsole(const std::vector<ais::AisLogEntry>& entries,
                       int scroll_offset, uint64_t total_messages,
                       float messages_per_second, int gain_tenths, bool agc,
                       size_t vessels);

    void renderPlot(const std::vector<ais::VesselState>& vessels,
                    int selected_index, float range_nm, bool station_valid,
                    double station_latitude, double station_longitude,
                    uint64_t total_messages, float messages_per_second,
                    int gain_tenths, bool agc, MapBackground* map = nullptr);

    void renderDossier(const ais::VesselState* vessel, size_t vessel_count,
                       uint64_t total_messages, float messages_per_second,
                       int gain_tenths, bool agc);

    void setUdpEnabled(bool enabled) { udp_enabled_ = enabled; }

    void renderHelp();

    void drawChar(int x, int y, char c, uint16_t color) override;
    void drawText(int x, int y, const char* text, uint16_t color) override;
    void drawTextBg(int x, int y, const char* text, uint16_t fg,
                    uint16_t bg) override;
    int getTextWidth(const char* text) const override;

    void drawSmallText(int x, int y, const char* text, uint16_t color);
    int smallTextWidth(const char* text) const;

    uint16_t backgroundColor() const { return COLOR_BLACK; }

private:
    static const int HEADER_Y = 2;
    static const int COL_HEADER_Y = 20;
    static const int COL_HEADER_H = 14;
    static const int LIST_Y = 34;
    static const int STATS_BAR_Y = 154;
    static const int ROW_HEIGHT = 14;
    static const int MAX_VISIBLE_ROWS = 8;

    static const int COL_MMSI_X = 2;
    static const int COL_NAME_X = 70;
    static const int COL_SOG_X = 160;
    static const int COL_COG_X = 196;
    static const int COL_NM_X = 232;
    static const int COL_AGE_X = 272;

    static const int SCOPE_HEADER_Y = 2;
    static const int SCOPE_BOTTOM_Y = 154;
    static const int SCOPE_RADAR_Y = 22;
    static const int SCOPE_RADAR_H = 132;
    static const int SCOPE_CENTER_X = 160;
    static const int SCOPE_CENTER_Y = 88;
    static const int SCOPE_RADIUS = 58;

    void renderListStatusBar(uint64_t total_messages, float messages_per_second,
                             size_t vessels);
    void renderListFooter(uint64_t total_messages, size_t vessels,
                          int gain_tenths, bool agc);
    void renderPlotStatusBar(float range_nm, size_t vessels,
                             uint64_t total_messages);
    void renderPlotFooter(const ais::VesselState* selected, float range_nm);

    bool udp_enabled_ = false;
};

#endif  // AIS_UI_RENDERER_H
