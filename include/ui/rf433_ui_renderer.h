#ifndef RF433_UI_RENDERER_H
#define RF433_UI_RENDERER_H

#include "rf433/rtl433_controller.h"
#include "ui/base_ui_renderer.h"

#include <string>
#include <vector>

/**
 * Rf433UIRenderer - 433 MHz sensor list matching ADSB chrome.
 * Columns adapt to the active type filter (ALL / TPMS / WX / OTHER).
 */
class Rf433UIRenderer : public BaseUIRenderer {
public:
    explicit Rf433UIRenderer(Framebuffer& fb);

    void renderList(const std::vector<rf433::Sensor>& sensors,
                    int selected_index, int scroll_offset, uint64_t total,
                    float rate, float frequency_mhz, rf433::Filter filter,
                    bool running, const std::string& error);

    void renderDetail(const rf433::Sensor& sensor);

    void renderHelp();

private:
    static const int HEADER_Y = 2;
    static const int COL_HEADER_Y = 20;
    static const int COL_HEADER_H = 14;
    static const int LIST_Y = 34;
    static const int STATS_BAR_Y = 154;
    static const int ROW_HEIGHT = 14;
    static const int MAX_VISIBLE_ROWS = 8;

    static const int COL_ID_X = 2;
    static const int COL_MODEL_X = 56;
    static const int COL_A_X = 150;
    static const int COL_B_X = 210;
    static const int COL_AGE_X = 268;
};

#endif  // RF433_UI_RENDERER_H
