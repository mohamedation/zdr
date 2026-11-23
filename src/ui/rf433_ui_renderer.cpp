#include "ui/rf433_ui_renderer.h"

#include "ui/ui_text.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

void clipField(char* out, size_t out_size, const std::string& value, int max_chars) {
    if (out_size == 0) return;
    const int n = std::min(max_chars, static_cast<int>(value.size()));
    if (n <= 0) {
        out[0] = '-';
        out[1] = '\0';
        return;
    }
    std::snprintf(out, out_size, "%.*s", n, value.c_str());
}

void formatAge(double last_seen, char* out, size_t size) {
    const long age =
        static_cast<long>(time(nullptr) - static_cast<time_t>(last_seen));
    if (age < 0) {
        std::snprintf(out, size, "  0s");
    } else if (age < 60) {
        std::snprintf(out, size, "%3lds", age);
    } else if (age < 3600) {
        std::snprintf(out, size, "%3ldm", age / 60);
    } else {
        std::snprintf(out, size, "%3ldh", age / 3600);
    }
}

const char* kindLabel(rf433::Kind kind) {
    switch (kind) {
        case rf433::Kind::Tpms: return "TPMS";
        case rf433::Kind::Weather: return "WX";
        case rf433::Kind::Other:
        default: return "OTH";
    }
}

}  // namespace

Rf433UIRenderer::Rf433UIRenderer(Framebuffer& fb) : BaseUIRenderer(fb) {}

void Rf433UIRenderer::renderList(
    const std::vector<rf433::Sensor>& sensors, int selected_index,
    int scroll_offset, uint64_t total, float rate, float frequency_mhz,
    rf433::Filter filter, bool running, const std::string& error) {
    fb_.clear(COLOR_BLACK);

    char title[40];
    if (running) {
        std::snprintf(title, sizeof(title), "433  %.1f/s", rate);
    } else {
        std::snprintf(title, sizeof(title), "433  OFF");
    }
    uiText().draw(fb_, 6, HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    char trk[48];
    std::snprintf(trk, sizeof(trk), "%s  Sens:%zu  Pkts:%llu",
                  rf433::filterLabel(filter), sensors.size(),
                  static_cast<unsigned long long>(total));
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - trk_w, HEADER_Y + 2, trk, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);
    uiText().draw(fb_, COL_ID_X, COL_HEADER_Y + 1, "ID", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_MODEL_X, COL_HEADER_Y + 1, "MODEL",
                  TextStyle::Caption, COLOR_ADSB_TEXT);

    const char* col_a = "VALUE";
    const char* col_b = "TYPE";
    if (filter == rf433::Filter::Tpms) {
        col_a = "PSI";
        col_b = "TEMP";
    } else if (filter == rf433::Filter::Weather) {
        col_a = "TEMP";
        col_b = "HUM";
    } else if (filter == rf433::Filter::All) {
        col_a = "VALUE";
        col_b = "KIND";
    }
    uiText().draw(fb_, COL_A_X, COL_HEADER_Y + 1, col_a, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_B_X, COL_HEADER_Y + 1, col_b, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_AGE_X, COL_HEADER_Y + 1, "AGE", TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    if (sensors.empty()) {
        const char* wait = error.empty() ? "WAITING FOR 433..." : error.c_str();
        const int w = uiText().width(wait, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, wait, TextStyle::Caption,
                      error.empty() ? COLOR_ADSB_TEXT : COLOR_ADSB_SELECTED);
    } else {
        const int visible = std::min(
            static_cast<int>(sensors.size()) - scroll_offset, MAX_VISIBLE_ROWS);
        for (int i = 0; i < visible; ++i) {
            const int idx = scroll_offset + i;
            const auto& s = sensors[static_cast<size_t>(idx)];
            const int row_y = LIST_Y + i * ROW_HEIGHT;
            const bool selected = (idx == selected_index);
            if (selected) {
                fb_.fillRect(0, row_y, 320, ROW_HEIGHT, COLOR_ADSB_SELECTED);
            }
            const uint16_t color =
                selected ? COLOR_BLACK : COLOR_ADSB_TEXT;
            const int text_y = row_y + 2;

            char field[32];
            clipField(field, sizeof(field), s.id, 8);
            uiText().draw(fb_, COL_ID_X, text_y, field, TextStyle::Caption,
                          color);
            clipField(field, sizeof(field), s.model, 14);
            uiText().draw(fb_, COL_MODEL_X, text_y, field, TextStyle::Caption,
                          color);

            if (filter == rf433::Filter::Tpms) {
                if (s.pressure_psi >= 0.0f) {
                    std::snprintf(field, sizeof(field), "%5.1f", s.pressure_psi);
                } else {
                    std::snprintf(field, sizeof(field), "   -");
                }
                uiText().draw(fb_, COL_A_X, text_y, field, TextStyle::Caption,
                              color);
                if (s.temperature_c > -900.0f) {
                    std::snprintf(field, sizeof(field), "%4.0fC",
                                  s.temperature_c);
                } else {
                    std::snprintf(field, sizeof(field), "   -");
                }
                uiText().draw(fb_, COL_B_X, text_y, field, TextStyle::Caption,
                              color);
            } else if (filter == rf433::Filter::Weather) {
                if (s.temperature_c > -900.0f) {
                    std::snprintf(field, sizeof(field), "%5.1fC",
                                  s.temperature_c);
                } else {
                    std::snprintf(field, sizeof(field), "   -");
                }
                uiText().draw(fb_, COL_A_X, text_y, field, TextStyle::Caption,
                              color);
                if (s.humidity_pct >= 0.0f) {
                    std::snprintf(field, sizeof(field), "%3.0f%%",
                                  s.humidity_pct);
                } else if (s.wind_kmh >= 0.0f) {
                    std::snprintf(field, sizeof(field), "%3.0fk", s.wind_kmh);
                } else {
                    std::snprintf(field, sizeof(field), "  -");
                }
                uiText().draw(fb_, COL_B_X, text_y, field, TextStyle::Caption,
                              color);
            } else {
                clipField(field, sizeof(field), s.summary, 10);
                uiText().draw(fb_, COL_A_X, text_y, field, TextStyle::Caption,
                              color);
                uiText().draw(fb_, COL_B_X, text_y, kindLabel(s.kind),
                              TextStyle::Caption, color);
            }

            formatAge(s.last_seen, field, sizeof(field));
            uiText().draw(fb_, COL_AGE_X, text_y, field, TextStyle::Caption,
                          color);
        }
    }

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    char bottom[64];
    std::snprintf(bottom, sizeof(bottom), "%.2fM  T Filter  Enter Det  R Retry",
                  frequency_mhz);
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, bottom, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void Rf433UIRenderer::renderDetail(const rf433::Sensor& sensor) {
    fb_.clear(COLOR_BLACK);

    uiText().draw(fb_, 6, HEADER_Y + 2, "433 DETAIL", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    char age[16];
    formatAge(sensor.last_seen, age, sizeof(age));
    const int aw = uiText().width(age, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - aw, HEADER_Y + 2, age, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);

    int y = LIST_Y;
    auto row = [&](const char* label, const char* value) {
        uiText().draw(fb_, 6, y, label, TextStyle::Caption, COLOR_ADSB_TEXT);
        uiText().draw(fb_, 80, y, value, TextStyle::Caption, COLOR_ADSB_TEXT);
        y += ROW_HEIGHT;
    };

    char buf[48];
    clipField(buf, sizeof(buf), sensor.model, 28);
    row("MODEL", buf);
    clipField(buf, sizeof(buf), sensor.id, 20);
    row("ID", buf);
    row("KIND", kindLabel(sensor.kind));

    if (sensor.pressure_psi >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.1f psi", sensor.pressure_psi);
        row("PRESS", buf);
    }
    if (sensor.temperature_c > -900.0f) {
        std::snprintf(buf, sizeof(buf), "%.1f C", sensor.temperature_c);
        row("TEMP", buf);
    }
    if (sensor.humidity_pct >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.0f %%", sensor.humidity_pct);
        row("HUM", buf);
    }
    if (sensor.wind_kmh >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.1f km/h", sensor.wind_kmh);
        row("WIND", buf);
    }
    if (sensor.rain_mm >= 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.1f mm", sensor.rain_mm);
        row("RAIN", buf);
    }
    if (sensor.battery_ok >= 0) {
        row("BATT", sensor.battery_ok ? "OK" : "LOW");
    }
    std::snprintf(buf, sizeof(buf), "%d", sensor.hits);
    row("HITS", buf);

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, "Esc List  T Filter",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
}

void Rf433UIRenderer::renderHelp() {
    fb_.fillRect(24, 28, 272, 114, COLOR_BLACK);
    fb_.drawRect(24, 28, 272, 114, COLOR_RADAR_GRID);
    uiText().draw(fb_, 36, 36, "433 KEYS", TextStyle::Caption,
                  COLOR_ADSB_SELECTED);
    uiText().draw(fb_, 36, 54, "Enter  detail / back", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 68, "Up/Dn  select", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 82, "T      ALL / TPMS / WX / OTHER",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 96, "R      restart rtl_433", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 110, "H      this help", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}
