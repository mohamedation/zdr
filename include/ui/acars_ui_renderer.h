#ifndef ACARS_UI_RENDERER_H
#define ACARS_UI_RENDERER_H

#include "acars/acars_controller.h"
#include "ui/base_ui_renderer.h"

#include <string>
#include <vector>

/**
 * AcarsUIRenderer - list/detail chrome matching ADSB / radar (UiText Caption,
 * flat black canvas, amber selection row only).
 */
class AcarsUIRenderer : public BaseUIRenderer {
public:
    explicit AcarsUIRenderer(Framebuffer& fb);

    void renderList(const std::vector<acars::Message>& messages,
                    int selected_index, int scroll_offset, uint64_t total,
                    float rate, const std::vector<float>& channels,
                    float gain_db, bool running, const std::string& error);

    void renderDetail(const acars::Message& message, int scroll);

    void renderHelp();

private:
    static const int HEADER_Y = 2;
    static const int COL_HEADER_Y = 20;
    static const int COL_HEADER_H = 14;
    static const int LIST_Y = 34;
    static const int STATS_BAR_Y = 154;
    static const int ROW_HEIGHT = 14;
    static const int MAX_VISIBLE_ROWS = 8;

    static const int COL_TIME_X = 2;
    static const int COL_FLIGHT_X = 58;
    static const int COL_TAIL_X = 118;
    static const int COL_LBL_X = 178;
    static const int COL_TEXT_X = 210;
};

#endif  // ACARS_UI_RENDERER_H
