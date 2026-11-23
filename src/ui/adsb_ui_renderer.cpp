#include "ui/adsb_ui_renderer.h"
#include "ui/aircraft_glyph.h"
#include "ui/ui_text.h"
#include "font.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

// Define static constants
const int ADSBUIRenderer::HEADER_Y;
const int ADSBUIRenderer::COL_HEADER_Y;
const int ADSBUIRenderer::COL_HEADER_H;
const int ADSBUIRenderer::LIST_Y;
const int ADSBUIRenderer::LIST_H;
const int ADSBUIRenderer::STATS_BAR_Y;
const int ADSBUIRenderer::STATS_BAR_H;
const int ADSBUIRenderer::ROW_HEIGHT;
const int ADSBUIRenderer::MAX_VISIBLE_ROWS;
const int ADSBUIRenderer::COL_ICAO_X;
const int ADSBUIRenderer::COL_ICAO_W;
const int ADSBUIRenderer::COL_CALL_X;
const int ADSBUIRenderer::COL_CALL_W;
const int ADSBUIRenderer::COL_ALT_X;
const int ADSBUIRenderer::COL_ALT_W;
const int ADSBUIRenderer::COL_SPD_X;
const int ADSBUIRenderer::COL_SPD_W;
const int ADSBUIRenderer::COL_HDG_X;
const int ADSBUIRenderer::COL_HDG_W;
const int ADSBUIRenderer::COL_DIST_X;
const int ADSBUIRenderer::COL_DIST_W;
const int ADSBUIRenderer::COL_SEEN_X;
const int ADSBUIRenderer::COL_SEEN_W;

ADSBUIRenderer::ADSBUIRenderer(Framebuffer& fb)
    : BaseUIRenderer(fb) {
}

ADSBUIRenderer::~ADSBUIRenderer() {
}

// ADSB tables / labels use UiText Mono and Caption styles.
void ADSBUIRenderer::drawChar(int x, int y, char c, uint16_t color) {
    if (c < 0x20 || c > 0x7E) return;
    char buf[2] = {c, '\0'};
    uiText().draw(fb_, x, y, buf, TextStyle::Mono, color);
}

void ADSBUIRenderer::drawText(int x, int y, const char* text, uint16_t color) {
    if (!text) return;
    uiText().draw(fb_, x, y, text, TextStyle::Mono, color);
}

void ADSBUIRenderer::drawSmallText(int x, int y, const char* text, uint16_t color) {
    if (!text) return;
    uiText().draw(fb_, x, y, text, TextStyle::Caption, color);
}

void ADSBUIRenderer::drawTextBg(int x, int y, const char* text, uint16_t fg, uint16_t bg) {
    if (!text) return;
    const int w = getTextWidth(text);
    const int h = uiText().lineHeight(TextStyle::Mono);
    fb_.fillRect(x, y, w, h, bg);
    drawText(x, y, text, fg);
}

int ADSBUIRenderer::getTextWidth(const char* text) const {
    if (!text) return 0;
    return uiText().width(text, TextStyle::Mono);
}

void ADSBUIRenderer::renderStatusBar(int aircraft_count,
                                     uint32_t message_count,
                                     float message_rate) {
    // Caption chrome on flat black — no bar fill (matches profile / radar).
    char title[40];
    snprintf(title, sizeof(title), "ADSB  %.0f/s", message_rate);
    uiText().draw(fb_, 6, HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    char trk[32];
    snprintf(trk, sizeof(trk), "Trk:%d  Msg:%u", aircraft_count, message_count);
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - trk_w, HEADER_Y + 2, trk, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void ADSBUIRenderer::renderColumnHeaders() {
    // Column labels on the shared canvas (no panel fill).
    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);

    drawText(COL_ICAO_X, COL_HEADER_Y + 1, "ICAO", COLOR_ADSB_TEXT);
    drawText(COL_CALL_X, COL_HEADER_Y + 1, "Call", COLOR_ADSB_TEXT);
    drawText(COL_ALT_X, COL_HEADER_Y + 1, "Alt", COLOR_ADSB_TEXT);
    drawText(COL_SPD_X, COL_HEADER_Y + 1, "Spd", COLOR_ADSB_TEXT);
    drawText(COL_HDG_X, COL_HEADER_Y + 1, "Hdg", COLOR_ADSB_TEXT);
    drawText(COL_DIST_X, COL_HEADER_Y + 1, "Dist", COLOR_ADSB_TEXT);
    drawText(COL_SEEN_X, COL_HEADER_Y + 1, "Seen", COLOR_ADSB_TEXT);
}

void ADSBUIRenderer::renderAircraftList(const std::vector<AircraftState>& aircraft,
                                        int selected_index,
                                        int scroll_offset) {
    const int visible_count =
        std::min((int)aircraft.size() - scroll_offset, MAX_VISIBLE_ROWS);

    for (int i = 0; i < visible_count; i++) {
        const int aircraft_idx = scroll_offset + i;
        if (aircraft_idx >= (int)aircraft.size()) break;

        const AircraftState& ac = aircraft[aircraft_idx];
        const int row_y = LIST_Y + (i * ROW_HEIGHT);
        const bool is_selected = (aircraft_idx == selected_index);

        // Only the selected row is highlighted (full width).
        if (is_selected) {
            fb_.fillRect(0, row_y, 320, ROW_HEIGHT, COLOR_ADSB_SELECTED);
        }

        const uint16_t text_color =
            is_selected ? COLOR_BLACK : COLOR_ADSB_TEXT;
        char buffer[16];
        const int text_y = row_y + 2;

        snprintf(buffer, sizeof(buffer), "%06X", ac.icao_addr);
        drawText(COL_ICAO_X, text_y, buffer, text_color);

        // Callsign — plain text; no amber chip unless the row is selected.
        if (ac.callsign[0] != '\0') {
            int callsign_len = strlen(ac.callsign);
            while (callsign_len > 0 && ac.callsign[callsign_len - 1] == ' ') {
                callsign_len--;
            }
            if (callsign_len > 0) {
                char callsign_buf[9];
                snprintf(callsign_buf, sizeof(callsign_buf), "%.*s",
                         callsign_len, ac.callsign);
                drawText(COL_CALL_X, text_y, callsign_buf, text_color);
            } else {
                snprintf(buffer, sizeof(buffer), "-------");
                drawText(COL_CALL_X, text_y, buffer, text_color);
            }
        } else {
            snprintf(buffer, sizeof(buffer), "-------");
            drawText(COL_CALL_X, text_y, buffer, text_color);
        }

        if (ac.position_valid && ac.altitude > 0) {
            snprintf(buffer, sizeof(buffer), "%5d", ac.altitude);
        } else {
            snprintf(buffer, sizeof(buffer), "    -");
        }
        drawText(COL_ALT_X, text_y, buffer, text_color);

        if (ac.velocity_valid && ac.speed > 0) {
            snprintf(buffer, sizeof(buffer), "%3d", ac.speed);
        } else {
            snprintf(buffer, sizeof(buffer), "  -");
        }
        drawText(COL_SPD_X, text_y, buffer, text_color);

        if (ac.velocity_valid && ac.heading >= 0) {
            snprintf(buffer, sizeof(buffer), "%03d", ac.heading);
        } else {
            snprintf(buffer, sizeof(buffer), "  -");
        }
        drawText(COL_HDG_X, text_y, buffer, text_color);

        formatDistance(ac.distance, buffer);
        drawText(COL_DIST_X, text_y, buffer, text_color);

        time_t now = time(nullptr);
        time_t elapsed = now - ac.last_seen;
        formatTimeSince(elapsed, buffer);
        drawText(COL_SEEN_X, text_y, buffer, text_color);
    }
}

void ADSBUIRenderer::renderStatistics(uint32_t total_messages,
                                      uint32_t crc_errors,
                                      float max_range) {
    // Bottom chrome — Caption on flat canvas (profile style).
    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);

    char stats[64];
    snprintf(stats, sizeof(stats), "Total:%u  CRC:%u  Range:%.0fkm",
             total_messages, crc_errors, max_range);
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, stats, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void ADSBUIRenderer::renderAircraftDetail(const AircraftState& aircraft,
                                          int scroll_offset,
                                          int aircraft_count,
                                          uint32_t message_count,
                                          float message_rate,
                                          float gain) {
    // Clear screen
    fb_.clear(COLOR_BG_DARK);

    // Define layout areas - give more space to PFD
    const int PFD_X = 0;
    const int PFD_Y = 0;
    const int PFD_W = 180;  // Increased from 140 to 180
    const int PFD_H = STATS_BAR_Y;  // 152px

    const int INFO_X = 180;  // Moved from 140 to 180
    const int INFO_Y = 0;
    const int INFO_W = 140;  // Reduced from 180 to 140
    const int INFO_H = STATS_BAR_Y;  // 152px

    // Render PFD on left side
    renderPFD(aircraft, PFD_X, PFD_Y, PFD_W, PFD_H);

    // Build detail text lines
    std::vector<std::string> lines;
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "ICAO: %06X", aircraft.icao_addr);
    lines.push_back(std::string(buffer));

    if (aircraft.callsign[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "Call: %s", aircraft.callsign);
        lines.push_back(std::string(buffer));
    }

    if (aircraft.position_valid) {
        snprintf(buffer, sizeof(buffer), "Lat: %.6f", aircraft.latitude);
        lines.push_back(std::string(buffer));
        snprintf(buffer, sizeof(buffer), "Lon: %.6f", aircraft.longitude);
        lines.push_back(std::string(buffer));
        snprintf(buffer, sizeof(buffer), "Alt: %d ft", aircraft.altitude);
        lines.push_back(std::string(buffer));
    }

    if (aircraft.velocity_valid) {
        snprintf(buffer, sizeof(buffer), "Spd: %d kt", aircraft.speed);
        lines.push_back(std::string(buffer));
        snprintf(buffer, sizeof(buffer), "Hdg: %d deg", aircraft.heading);
        lines.push_back(std::string(buffer));
        snprintf(buffer, sizeof(buffer), "V/S: %d fpm", aircraft.vertical_rate);
        lines.push_back(std::string(buffer));
    }

    if (aircraft.distance > 0) {
        snprintf(buffer, sizeof(buffer), "Dist: %.1f km", aircraft.distance);
        lines.push_back(std::string(buffer));
        snprintf(buffer, sizeof(buffer), "Bear: %.0f deg", aircraft.bearing);
        lines.push_back(std::string(buffer));
    }

    snprintf(buffer, sizeof(buffer), "Msgs: %u", aircraft.message_count);
    lines.push_back(std::string(buffer));

    time_t now = time(nullptr);
    time_t elapsed = now - aircraft.last_seen;
    snprintf(buffer, sizeof(buffer), "Seen: %ld s", (long)elapsed);
    lines.push_back(std::string(buffer));

    // Clear info area
    fb_.fillRect(INFO_X, INFO_Y, INFO_W, INFO_H, COLOR_BG_DARK);

    // Render scrollable text on right side
    const int LINE_HEIGHT = uiText().lineHeight(TextStyle::Body) + 3;
    int y = INFO_Y + 2 - scroll_offset;

    for (size_t i = 0; i < lines.size(); i++) {
        if (y + LINE_HEIGHT > INFO_Y && y < STATS_BAR_Y) {
            uiText().draw(fb_, INFO_X + 5, y, lines[i].c_str(), TextStyle::Body,
                          COLOR_ADSB_TEXT);
        }
        y += LINE_HEIGHT;
    }

    // Render bottom status bar
    fb_.fillRect(0, STATS_BAR_Y, 320, STATS_BAR_H, COLOR_ADSB_STATUSBAR_BG);
    drawText(2, STATS_BAR_Y + 2, "Enter:Radar  Up/Down:Scroll", COLOR_ADSB_STATUSBAR_TEXT);
}

void ADSBUIRenderer::drawDashedHLine(int x, int y, int length, uint16_t color,
                                     int dash, int gap) {
    if (length <= 0 || dash <= 0) return;
    int cursor = x;
    const int end = x + length;
    while (cursor < end) {
        const int seg = std::min(dash, end - cursor);
        drawHLine(cursor, y, seg, color);
        cursor += seg + gap;
    }
}

void ADSBUIRenderer::drawProfileTrendArrow(int x, int y, int direction,
                                           uint16_t color) {
    // direction > 0 = climb (up), < 0 = descend (down)
    if (direction > 0) {
        fb_.drawPixel(x, y - 3, color);
        drawHLine(x - 1, y - 2, 3, color);
        drawHLine(x - 2, y - 1, 5, color);
        drawVLine(x, y, 4, color);
    } else if (direction < 0) {
        drawVLine(x, y - 3, 4, color);
        drawHLine(x - 2, y + 1, 5, color);
        drawHLine(x - 1, y + 2, 3, color);
        fb_.drawPixel(x, y + 3, color);
    }
}

void ADSBUIRenderer::renderVerticalProfile(
    const std::vector<AircraftState>& aircraft, int selected_index,
    float max_range_km) {
    constexpr int kW = 320;
    constexpr int kH = 170;
    constexpr int kBorder = 1;
    constexpr int kHeaderY = 2;
    constexpr int kBottomY = 154;
    constexpr int kPlotL = 40;   // room for altitude labels
    constexpr int kPlotR = 316;
    constexpr int kPlotT = 24;   // clear of title row (no header rule)
    constexpr int kPlotB = 138;  // axis just above bottom chrome
    constexpr int kMinIconGapX = 14;  // clear air between compact glyphs
    // Match radar inner-ring dash (3 on / 3 off).
    constexpr int kDash = 3;
    constexpr int kGap = 3;

    if (max_range_km < 10.0f) max_range_km = 10.0f;

    fb_.clear(COLOR_BLACK);

    // Header (no rule under title / track count)
    uiText().draw(fb_, 6, kHeaderY + 2, "VERTICAL PROFILE", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    char trk[16];
    snprintf(trk, sizeof(trk), "Trk: %d", static_cast<int>(aircraft.size()));
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, kW - 8 - trk_w, kHeaderY + 2, trk, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    const int plot_w = kPlotR - kPlotL;
    const int plot_h = kPlotB - kPlotT;

    // Altitude envelope from in-range traffic (same filter as plotted marks).
    float data_min = 0.0f;
    float data_max = 0.0f;
    bool have_alt = false;
    for (const auto& ac : aircraft) {
        if (!ac.position_valid || ac.distance <= 0.0f) continue;
        if (ac.distance > max_range_km) continue;
        const float alt =
            (ac.altitude > 0) ? static_cast<float>(ac.altitude) : 0.0f;
        if (!have_alt) {
            data_min = data_max = alt;
            have_alt = true;
        } else {
            data_min = std::min(data_min, alt);
            data_max = std::max(data_max, alt);
        }
    }

    // Grid lines only on FL100 steps (FL100, FL200, …). First line is at
    // least FL100 so low traffic can sit below it without its own line.
    // Highest line is ceil(max FL / 100) * 100 (e.g. FL410 → FL500).
    constexpr float kFlStep = 100.0f;
    constexpr float kMinFirstFl = 100.0f;

    float first_fl = kMinFirstFl;
    float last_fl = 400.0f;
    if (have_alt) {
        const float min_fl = data_min / 100.0f;
        const float max_fl = data_max / 100.0f;
        first_fl = std::floor(min_fl / kFlStep) * kFlStep;
        if (first_fl < kMinFirstFl) first_fl = kMinFirstFl;
        last_fl = std::ceil(max_fl / kFlStep) * kFlStep;
        if (last_fl < first_fl) last_fl = first_fl;
    }

    const float first_ft = first_fl * 100.0f;
    const float last_ft = last_fl * 100.0f;

    // Linear scale spans the labeled FL band and any traffic outside it.
    float alt_lo = first_ft;
    float alt_hi = last_ft;
    if (have_alt) {
        if (data_min < first_ft) {
            alt_lo = std::max(0.0f, data_min - 800.0f);
        }
        if (data_max > last_ft) {
            alt_hi = data_max + 800.0f;
        } else {
            alt_hi = last_ft;
        }
        // Keep a little headroom above the top grid line when traffic sits on it.
        if (data_max >= last_ft - 200.0f) {
            alt_hi = std::max(alt_hi, last_ft + 800.0f);
        }
    }
    if (alt_hi <= alt_lo) alt_hi = alt_lo + 10000.0f;

    auto altToY = [&](float alt_ft) -> int {
        if (alt_ft < alt_lo) alt_ft = alt_lo;
        if (alt_ft > alt_hi) alt_ft = alt_hi;
        const float t = (alt_ft - alt_lo) / (alt_hi - alt_lo);
        return kPlotB - static_cast<int>(t * plot_h + 0.5f);
    };
    auto distToX = [&](float dist_km) -> int {
        if (dist_km < 0.0f) dist_km = 0.0f;
        if (dist_km > max_range_km) dist_km = max_range_km;
        const float t = dist_km / max_range_km;
        return kPlotL + static_cast<int>(t * plot_w + 0.5f);
    };

    // Equally spaced FL100 grid (radar-style dashes). No per-craft lines.
    for (float fl = first_fl; fl <= last_fl + 0.5f; fl += kFlStep) {
        const float alt_ft = fl * 100.0f;
        const int y = altToY(alt_ft);
        drawDashedHLine(kPlotL, y, plot_w, COLOR_RADAR_GRID, kDash, kGap);
        char label[12];
        snprintf(label, sizeof(label), "FL%03.0f", fl);
        const int label_w = uiText().width(label, TextStyle::Caption);
        uiText().draw(fb_, kPlotL - 4 - label_w, y - 4, label,
                      TextStyle::Caption, COLOR_ADSB_TEXT);
    }

    // X axis + distance ticks
    drawHLine(kPlotL, kPlotB, plot_w, COLOR_ADSB_TEXT);
    const int tick_count = 3;
    for (int i = 0; i <= tick_count; ++i) {
        const float dist = max_range_km * (static_cast<float>(i) / tick_count);
        const int x = distToX(dist);
        drawVLine(x, kPlotB, 3, COLOR_ADSB_TEXT);

        char label[12];
        if (i == tick_count) {
            snprintf(label, sizeof(label), "%.0f km", max_range_km);
        } else {
            snprintf(label, sizeof(label), "%.0f", dist);
        }
        const int lw = uiText().width(label, TextStyle::Caption);
        int lx = x - lw / 2;
        if (lx < kPlotL) lx = kPlotL;
        if (lx + lw > kPlotR) lx = kPlotR - lw;
        uiText().draw(fb_, lx, kPlotB + 4, label, TextStyle::Caption,
                      COLOR_ADSB_TEXT);
    }

    struct PlotMark {
        int idx;
        int x;
        int y;
        int vs_dir;  // -1 / 0 / +1
        bool selected;
    };
    std::vector<PlotMark> marks;
    marks.reserve(aircraft.size());

    for (size_t i = 0; i < aircraft.size(); ++i) {
        const AircraftState& ac = aircraft[i];
        if (!ac.position_valid || ac.distance <= 0.0f) continue;
        if (ac.distance > max_range_km) continue;

        const float alt =
            (ac.altitude > 0) ? static_cast<float>(ac.altitude) : 0.0f;
        int vs_dir = 0;
        if (ac.velocity_valid && std::abs(ac.vertical_rate) >= 200) {
            vs_dir = (ac.vertical_rate > 0) ? 1 : -1;
        }
        marks.push_back(PlotMark{
            static_cast<int>(i),
            distToX(ac.distance),
            altToY(alt),
            vs_dir,
            static_cast<int>(i) == selected_index,
        });
    }

    // Keep left-to-right order by true distance, then spread so glyphs don't
    // stack. When the range is dense, use even slots across the plot width.
    std::sort(marks.begin(), marks.end(),
              [](const PlotMark& a, const PlotMark& b) { return a.x < b.x; });
    const int n = static_cast<int>(marks.size());
    const int usable = plot_w - 12;
    if (n >= 2 && n * kMinIconGapX > usable) {
        for (int i = 0; i < n; ++i) {
            marks[i].x = kPlotL + 6 + (i * usable) / (n - 1);
        }
    } else {
        for (int pass = 0; pass < 6; ++pass) {
            for (size_t i = 1; i < marks.size(); ++i) {
                PlotMark& prev = marks[i - 1];
                PlotMark& cur = marks[i];
                const int gap = cur.x - prev.x;
                if (gap >= kMinIconGapX) continue;
                const int need = kMinIconGapX - gap;
                const int push_l = need / 2;
                const int push_r = need - push_l;
                prev.x = std::max(kPlotL + 6, prev.x - push_l);
                cur.x = std::min(kPlotR - 6, cur.x + push_r);
                if (cur.x - prev.x < kMinIconGapX) {
                    cur.x = std::min(kPlotR - 6, prev.x + kMinIconGapX);
                }
            }
            std::sort(marks.begin(), marks.end(), [](const PlotMark& a,
                                                     const PlotMark& b) {
                return a.x < b.x;
            });
        }
    }

    auto drawMark = [&](const PlotMark& m) {
        const uint16_t color =
            m.selected ? COLOR_ADSB_SELECTED : COLOR_ADSB_TEXT;
        // Same silhouette as radar, scaled down for the dense profile plot.
        AircraftGlyph::draw(fb_, m.x, m.y, color, /*heading_deg=*/0.0f,
                            /*scale=*/0.65f);
        if (m.vs_dir != 0) {
            drawProfileTrendArrow(m.x + 8, m.y, m.vs_dir, color);
        }
    };

    for (const auto& m : marks) {
        if (!m.selected) drawMark(m);
    }
    for (const auto& m : marks) {
        if (m.selected) drawMark(m);
    }

    // Bottom chrome — callsign only (no "Sel:" prefix).
    fb_.fillRect(kBorder, kBottomY, kW - 2 * kBorder, kH - kBottomY - kBorder,
                 COLOR_BLACK);
    drawHLine(kBorder + 2, kBottomY, kW - 2 * kBorder - 4, COLOR_RADAR_GRID);

    char sel[16];
    sel[0] = '\0';
    if (selected_index >= 0 &&
        selected_index < static_cast<int>(aircraft.size())) {
        const AircraftState& ac = aircraft[selected_index];
        int n = 0;
        for (; n < 8 && ac.callsign[n] && ac.callsign[n] != ' '; ++n) {
            sel[n] = ac.callsign[n];
        }
        sel[n] = '\0';
        if (n == 0) {
            snprintf(sel, sizeof(sel), "%06X", ac.icao_addr);
        }
    } else {
        snprintf(sel, sizeof(sel), "---");
    }
    uiText().draw(fb_, 6, kBottomY + 3, sel, TextStyle::Caption,
                  COLOR_ADSB_SELECTED);

    const char* hints = "Up/Dn Change  M Menu";
    const int hints_w = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, kW - 8 - hints_w, kBottomY + 3, hints,
                  TextStyle::Caption, COLOR_ADSB_TEXT);
}

void ADSBUIRenderer::formatTimeSince(time_t seconds, char* buffer) {
    if (seconds < 60) {
        snprintf(buffer, 5, "%2lds", (long)seconds);
    } else if (seconds < 3600) {
        snprintf(buffer, 5, "%2ldm", (long)(seconds / 60));
    } else {
        snprintf(buffer, 5, "%2ldh", (long)(seconds / 3600));
    }
}

void ADSBUIRenderer::formatDistance(float distance, char* buffer) {
    if (distance < 0.1f) {
        snprintf(buffer, 6, "   -");
    } else if (distance < 10.0f) {
        snprintf(buffer, 6, "%4.1fk", distance);
    } else if (distance < 100.0f) {
        snprintf(buffer, 6, "%4.0fk", distance);
    } else {
        snprintf(buffer, 6, " >99k");
    }
}

// PFD rendering implementation

void ADSBUIRenderer::drawCircleApprox(int cx, int cy, int radius, uint16_t color) {
    // Bresenham's circle algorithm
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

float ADSBUIRenderer::calculatePitchAngle(const AircraftState& aircraft) {
    // Need valid velocity data and reasonable speed
    if (!aircraft.velocity_valid || aircraft.speed < 50) {
        return 0.0f;
    }

    // Convert vertical rate from feet/min to feet/sec
    float vr_fps = aircraft.vertical_rate / 60.0f;

    // Convert ground speed from knots to feet/sec
    // 1 knot = 1.68781 feet/sec
    float gs_fps = aircraft.speed * 1.68781f;

    // Calculate pitch angle (flight path angle)
    float pitch_rad = atan2f(vr_fps, gs_fps);
    float pitch_deg = pitch_rad * 180.0f / M_PI;

    // Clamp to reasonable display range
    if (pitch_deg > 15.0f) pitch_deg = 15.0f;
    if (pitch_deg < -15.0f) pitch_deg = -15.0f;

    return pitch_deg;
}

void ADSBUIRenderer::drawArtificialHorizon(int cx, int cy, int radius, float pitch_angle) {
    const float pixels_per_degree = 2.5f;

    // Calculate vertical offset for horizon line
    // Positive pitch (nose up) = horizon moves down on display
    int y_offset = (int)(pitch_angle * pixels_per_degree);

    // Clamp offset to keep horizon within visible circle
    int max_offset = radius - 5;
    if (y_offset > max_offset) y_offset = max_offset;
    if (y_offset < -max_offset) y_offset = -max_offset;

    int horizon_y = cy + y_offset;

    // Define clipping width (two-sided cut)
    int clip_width = radius * 1.6;
    int clip_left = cx - clip_width/2;
    int clip_right = cx + clip_width/2;

    // Fill sky (above horizon line) with bright sky blue
    for (int dy = -radius; dy <= radius; dy++) {
        int current_y = cy + dy;

        // Skip if below horizon
        if (current_y >= horizon_y) continue;

        int dx = (int)sqrt(radius * radius - dy * dy);
        int x_start = std::max(clip_left, cx - dx);
        int x_end = std::min(clip_right, cx + dx);
        if (x_start < x_end) {
            fb_.drawLine(x_start, current_y, x_end, current_y, COLOR_SKY_BLUE);
        }
    }

    // Fill ground (below horizon line) with earth brown
    for (int dy = -radius; dy <= radius; dy++) {
        int current_y = cy + dy;

        // Skip if above horizon
        if (current_y < horizon_y) continue;

        int dx = (int)sqrt(radius * radius - dy * dy);
        int x_start = std::max(clip_left, cx - dx);
        int x_end = std::min(clip_right, cx + dx);
        if (x_start < x_end) {
            fb_.drawLine(x_start, current_y, x_end, current_y, COLOR_EARTH_BROWN);
        }
    }

    // Draw horizon line at offset position (clipped, white)
    fb_.drawLine(clip_left, horizon_y, clip_right, horizon_y, COLOR_WHITE);

    // Draw aircraft symbol (yellow cross) - always at center (fixed reference)
    fb_.drawLine(cx - 15, cy, cx - 5, cy, COLOR_YELLOW);
    fb_.drawLine(cx + 5, cy, cx + 15, cy, COLOR_YELLOW);
    fb_.drawLine(cx, cy - 5, cx, cy + 5, COLOR_YELLOW);
}

void ADSBUIRenderer::drawPitchLadder(int cx, int cy, int width, int height, float current_pitch) {
    const float pixels_per_degree = 2.5f;

    // Calculate offset for current pitch
    // This makes the pitch ladder move with the horizon
    int pitch_offset = (int)(current_pitch * pixels_per_degree);

    // Draw pitch marks every 5 degrees from -30 to +30
    for (int pitch = -30; pitch <= 30; pitch += 5) {
        if (pitch == 0) continue;  // Skip horizon line (drawn separately)

        // Calculate mark position relative to current pitch
        int y_offset = -pitch * pixels_per_degree + pitch_offset;
        int mark_y = cy + y_offset;

        // Skip if outside visible area
        if (mark_y < cy - height/2 || mark_y > cy + height/2) continue;

        // Longer marks for 10-degree intervals
        int mark_width = (pitch % 10 == 0) ? 14 : 10;

        // Draw left and right pitch marks (green)
        fb_.drawLine(cx - width/2, mark_y, cx - width/2 + mark_width, mark_y, COLOR_GREEN);
        fb_.drawLine(cx + width/2 - mark_width, mark_y, cx + width/2, mark_y, COLOR_GREEN);

            // Draw text labels only for 10-degree intervals
            if (pitch % 10 == 0) {
                char label[8];
                snprintf(label, sizeof(label), "%d", abs(pitch));

                // Left label - pushed outward toward speed tape
                drawSmallText(cx - width/2 - 22, mark_y - 3, label, COLOR_WHITE);

                // Right label - pushed outward toward altitude tape
                drawSmallText(cx + width/2 + 8, mark_y - 3, label, COLOR_WHITE);
            }
    }
}

void ADSBUIRenderer::drawBankAngleIndicator(int cx, int cy, int radius) {
    // Draw bank angle scale arc at bottom
    const int bank_angles[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};

    for (int angle : bank_angles) {
        // Convert angle to radians
        float rad = angle * M_PI / 180.0f;

        // Calculate mark position on bottom arc
        int mark_x = cx + (int)(radius * sin(rad));
        int mark_y = cy + (int)(radius * cos(rad));

        // Draw tick mark (longer for major angles)
        int tick_length = (angle % 30 == 0) ? 5 : 3;
        int tick_end_x = cx + (int)((radius + tick_length) * sin(rad));
        int tick_end_y = cy + (int)((radius + tick_length) * cos(rad));

        fb_.drawLine(mark_x, mark_y, tick_end_x, tick_end_y, COLOR_GREEN);

        // Draw text labels for major angles (30, 60)
        // Center text on the tick mark for left/right symmetry
        if (angle % 30 == 0 && angle != 0) {
            char label[8];
            snprintf(label, sizeof(label), "%d", abs(angle));
            int text_x = mark_x - 7;  // 14px text width / 2
            int text_y = mark_y - 4;  // 8px font height / 2
            drawSmallText(text_x, text_y, label, COLOR_WHITE);
        }
    }

    // Draw center triangle pointer at top (0°) - this is the fixed reference
    fb_.drawLine(cx, cy - radius - 4, cx - 2, cy - radius, COLOR_WHITE);
    fb_.drawLine(cx, cy - radius - 4, cx + 2, cy - radius, COLOR_WHITE);
    fb_.drawLine(cx - 2, cy - radius, cx + 2, cy - radius, COLOR_WHITE);
}

void ADSBUIRenderer::drawAltitudeTape(int x, int y, int w, int h, int altitude) {
    // Draw tape background
    fb_.fillRect(x, y, w, h, 0x2104);  // Dark background
    fb_.drawRect(x, y, w, h, COLOR_GRID);
    
    // Draw altitude value in center
    char alt_str[8];
    snprintf(alt_str, sizeof(alt_str), "%d", altitude);
    int text_y = y + h/2 - 8;
    fb_.fillRect(x + 1, text_y, w - 2, 16, COLOR_BLACK);
    drawText(x + 2, text_y, alt_str, COLOR_GREEN);
    
    // Draw tick marks (every 1000 ft)
    int alt_base = (altitude / 1000) * 1000;
    for (int i = -2; i <= 2; i++) {
        int tick_alt = alt_base + i * 1000;
        int tick_y = y + h/2 - ((altitude - tick_alt) * h / 5000);
        if (tick_y >= y && tick_y <= y + h) {
            fb_.drawLine(x, tick_y, x + 5, tick_y, COLOR_GRID);
        }
    }
}

void ADSBUIRenderer::drawSpeedTape(int x, int y, int w, int h, int speed) {
    // Draw tape background
    fb_.fillRect(x, y, w, h, 0x2104);
    fb_.drawRect(x, y, w, h, COLOR_GRID);
    
    // Draw speed value in center
    char spd_str[8];
    snprintf(spd_str, sizeof(spd_str), "%d", speed);
    int text_y = y + h/2 - 8;
    fb_.fillRect(x + 1, text_y, w - 2, 16, COLOR_BLACK);
    drawText(x + 2, text_y, spd_str, COLOR_GREEN);
    
    // Draw tick marks (every 50 kt)
    int spd_base = (speed / 50) * 50;
    for (int i = -2; i <= 2; i++) {
        int tick_spd = spd_base + i * 50;
        if (tick_spd < 0) continue;
        int tick_y = y + h/2 - ((speed - tick_spd) * h / 250);
        if (tick_y >= y && tick_y <= y + h) {
            fb_.drawLine(x + w - 5, tick_y, x + w, tick_y, COLOR_GRID);
        }
    }
}

// Helper function to get direction label for a given heading
static const char* getDirectionLabel(int heading) {
    // Normalize heading to 0-359 range
    heading = ((heading % 360) + 360) % 360;

    if (heading >= 337 || heading < 23) return "N";
    if (heading >= 23 && heading < 68) return "NE";
    if (heading >= 68 && heading < 113) return "E";
    if (heading >= 113 && heading < 158) return "SE";
    if (heading >= 158 && heading < 203) return "S";
    if (heading >= 203 && heading < 248) return "SW";
    if (heading >= 248 && heading < 293) return "W";
    if (heading >= 293 && heading < 337) return "NW";
    return "N";
}

void ADSBUIRenderer::drawHeadingIndicator(int x, int y, int w, int h, int heading) {
    // Draw tape background
    fb_.fillRect(x, y, w, h, 0x2104);  // Dark background
    fb_.drawRect(x, y, w, h, COLOR_GRID);

    // Calculate center position
    int cx = x + w / 2;
    int cy = y + h / 2;

    // Visible range: ±60° from current heading (120° total)
    const int visible_range = 120;
    const float pixels_per_degree = (float)w / (float)visible_range;

    // Draw major direction labels (every 45°)
    for (int hdg = 0; hdg < 360; hdg += 45) {
        // Calculate angular difference (handle wraparound)
        int diff = hdg - heading;
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;

        // Skip if outside visible range
        if (diff < -60 || diff > 60) continue;

        // Calculate x position
        int tick_x = cx + (int)(diff * pixels_per_degree);

        // Draw direction label in middle area
        const char* dir_label = "";
        if (hdg == 0 || hdg == 360) dir_label = "N";
        else if (hdg == 45) dir_label = "NE";
        else if (hdg == 90) dir_label = "E";
        else if (hdg == 135) dir_label = "SE";
        else if (hdg == 180) dir_label = "S";
        else if (hdg == 225) dir_label = "SW";
        else if (hdg == 270) dir_label = "W";
        else if (hdg == 315) dir_label = "NW";

        int label_w = strlen(dir_label) * (FONT_6X8_WIDTH + 1);
        drawSmallText(tick_x - label_w/2, cy - 3, dir_label, COLOR_WHITE);

        // Draw tick mark at bottom
        fb_.drawLine(tick_x, y + h - 6, tick_x, y + h - 1, COLOR_GRID);
    }

    // Draw minor tick marks (every 15°) without labels
    for (int hdg = 0; hdg < 360; hdg += 15) {
        // Skip major directions (already drawn)
        if (hdg % 45 == 0) continue;

        // Calculate angular difference (handle wraparound)
        int diff = hdg - heading;
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;

        // Skip if outside visible range
        if (diff < -60 || diff > 60) continue;

        // Calculate x position
        int tick_x = cx + (int)(diff * pixels_per_degree);

        // Draw shorter tick mark at bottom
        fb_.drawLine(tick_x, y + h - 3, tick_x, y + h - 1, COLOR_GRID);
    }

    // Draw fixed triangle pointer at top center (aircraft reference)
    fb_.drawLine(cx, y + 1, cx - 3, y + 5, COLOR_MAGENTA);
    fb_.drawLine(cx, y + 1, cx + 3, y + 5, COLOR_MAGENTA);
    fb_.drawLine(cx - 3, y + 5, cx + 3, y + 5, COLOR_MAGENTA);
}

void ADSBUIRenderer::drawVerticalSpeedIndicator(int x, int y, int w, int h, int vs) {
    // No border - removed for cleaner look

    // Calculate center position (where altitude value is displayed)
    int cy = y + h/2;

    if (vs > 100) {
        // Climbing - green arrow up (above altitude value)
        int arrow_y = cy - 12;  // Position above altitude text
        // Draw upward arrow
        fb_.drawLine(x, arrow_y, x, arrow_y - 6, COLOR_GREEN);  // Vertical line
        fb_.drawLine(x, arrow_y - 6, x - 2, arrow_y - 3, COLOR_GREEN);  // Left wing
        fb_.drawLine(x, arrow_y - 6, x + 2, arrow_y - 3, COLOR_GREEN);  // Right wing
    } else if (vs < -100) {
        // Descending - red arrow down (below altitude value)
        int arrow_y = cy + 12;  // Position below altitude text
        // Draw downward arrow
        fb_.drawLine(x, arrow_y, x, arrow_y + 6, COLOR_RED);  // Vertical line
        fb_.drawLine(x, arrow_y + 6, x - 2, arrow_y + 3, COLOR_RED);  // Left wing
        fb_.drawLine(x, arrow_y + 6, x + 2, arrow_y + 3, COLOR_RED);  // Right wing
    }
}

void ADSBUIRenderer::renderPFD(const AircraftState& aircraft, int x, int y, int w, int h) {
    // Clear PFD area
    fb_.fillRect(x, y, w, h, COLOR_BG_DARK);

    // No border - removed for cleaner look

    // Layout within PFD area
    const int MARGIN = 2;
    const int TAPE_WIDTH = 25;

    // Artificial horizon in center
    int horizon_cx = x + w/2;
    int horizon_cy = y + h/2;
    int horizon_radius = 45;  // Increased from 40 to 45 for more space

    // Calculate pitch angle from aircraft data
    float pitch_angle = calculatePitchAngle(aircraft);

    if (aircraft.velocity_valid || aircraft.position_valid) {
        drawArtificialHorizon(horizon_cx, horizon_cy, horizon_radius, pitch_angle);
        drawPitchLadder(horizon_cx, horizon_cy, horizon_radius * 1.8, horizon_radius * 2, pitch_angle);
        drawBankAngleIndicator(horizon_cx, horizon_cy, horizon_radius);
    }
    
    // Speed tape on left
    if (aircraft.velocity_valid && aircraft.speed > 0) {
        drawSpeedTape(x + MARGIN, y + 20, TAPE_WIDTH, h - 60, aircraft.speed);
    }
    
    // Altitude tape on right
    if (aircraft.position_valid && aircraft.altitude > 0) {
        drawAltitudeTape(x + w - TAPE_WIDTH - MARGIN, y + 20, TAPE_WIDTH, h - 60, aircraft.altitude);
    }
    
    // Heading tape at bottom
    if (aircraft.velocity_valid && aircraft.heading >= 0) {
        int hdg_y = y + h - 25;
        int hdg_w = 140;  // Tape width
        int hdg_h = 22;   // Tape height
        drawHeadingIndicator(horizon_cx - hdg_w/2, hdg_y, hdg_w, hdg_h, aircraft.heading);
    }
    
    // Vertical speed indicator (small, right of altitude tape)
    if (aircraft.velocity_valid) {
        int vs_x = x + w - 10;
        int vs_y = y + 20;
        int vs_h = h - 60;
        drawVerticalSpeedIndicator(vs_x, vs_y, 8, vs_h, aircraft.vertical_rate);
    }
    
    // Callsign at top
    if (aircraft.callsign[0] != '\0') {
        const int text_w = uiText().width(aircraft.callsign, TextStyle::Title);
        uiText().draw(fb_, horizon_cx - text_w / 2, y + 2, aircraft.callsign,
                      TextStyle::Title, COLOR_ADSB_TEXT);
    }
}

void ADSBUIRenderer::renderText(int x, int y, const char* text, uint16_t color, bool bold) {
    // Simply delegate to drawText (bold parameter ignored)
    drawText(x, y, text, color);
}

void ADSBUIRenderer::drawRoundedPanel(int x, int y, int w, int h, int radius,
                                      uint16_t color, float alpha) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w / 2, h / 2)));
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            const float dy = radius - (row + 0.5f);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            const float dy = (row + 0.5f) - (h - radius);
            inset = radius - static_cast<int>(std::lround(
                                 std::sqrt(radius * radius - dy * dy)));
        }
        for (int col = 0; col < w - 2 * inset; ++col) {
            fb_.blendPixel(x + inset + col, y + row, color, alpha);
        }
    }
}

void ADSBUIRenderer::renderKeyHelp() {
    // Dim the page behind the translucent help panel.
    for (int y = 0; y < 170; ++y) {
        for (int x = 0; x < 320; ++x) {
            fb_.blendPixel(x, y, COLOR_BLACK, 0.45f);
        }
    }

    const int panel_w = 252;
    const int panel_h = 152;
    const int radius = 9;
    const int px = (320 - panel_w) / 2;
    const int py = (170 - panel_h) / 2;
    drawRoundedPanel(px, py, panel_w, panel_h, radius, COLOR_DIALOG_BG, 0.85f);

    drawSmallText(px + 12, py + 9, "KEYS", COLOR_ADSB_SELECTED);
    drawSmallText(px + 60, py + 9, "H close   <> settings",
                  COLOR_TEXT_DIM);

    struct Row { const char* key; const char* action; };
    static const Row rows[] = {
        {"TAB",        "PAGE MENU"},
        {"UP/DOWN",    "SELECT AIRCRAFT"},
        {"LEFT/RIGHT", "PAGE / ZOOM SCOPE"},
        {"ENTER",      "LIST>DET>RADAR>SCOPE>PROF"},
        {"R",          "RADAR / SCOPE->RADAR"},
        {"I / 0",      "COMPASS UP / NORTH"},
        {"L",          "POSITION SETTINGS"},
        {"M",          "PROFILE HELP MENU"},
        {"H",          "HELP / SETTINGS"},
        {"G",          "GAIN MODE (RADAR)"},
        {"+ / -",      "GAIN (RADAR)"},
        {"FN+D/S",     "VOLUME"},
        {"HOLD ESC",   "QUIT"},
    };

    const int count = (int)(sizeof(rows) / sizeof(rows[0]));
    const int step = count > 11 ? 10 : 11;
    int y = py + 24;
    for (int i = 0; i < count; ++i) {
        const auto& r = rows[i];
        drawSmallText(px + 14, y, r.key, COLOR_ADSB_SELECTED);
        drawSmallText(px + 92, y, r.action, COLOR_ADSB_TEXT);
        y += step;
    }
}

void ADSBUIRenderer::renderSettingsOverview(double lat, double lon,
                                            bool pos_set, int selected,
                                            const char* source) {
    for (int y = 0; y < 170; ++y) {
        for (int x = 0; x < 320; ++x) {
            fb_.blendPixel(x, y, COLOR_BLACK, 0.45f);
        }
    }

    const int panel_w = 260;
    const int panel_h = 146;
    const int radius = 9;
    const int px = (320 - panel_w) / 2;
    const int py = (170 - panel_h) / 2;
    drawRoundedPanel(px, py, panel_w, panel_h, radius, COLOR_DIALOG_BG, 0.85f);

    drawSmallText(px + 12, py + 9, "SETTINGS", COLOR_ADSB_SELECTED);
    drawSmallText(px + 80, py + 9, "H close   <> keys", COLOR_TEXT_DIM);

    char lat_s[32];
    char lon_s[32];
    if (pos_set) {
        snprintf(lat_s, sizeof(lat_s), "%.4f", lat);
        snprintf(lon_s, sizeof(lon_s), "%.4f", lon);
    } else {
        snprintf(lat_s, sizeof(lat_s), "NOT SET");
        snprintf(lon_s, sizeof(lon_s), "NOT SET");
    }

    static const char* labels[3] = {"LATITUDE", "LONGITUDE", "CLEAR POSITION"};
    const char* values[3] = {lat_s, lon_s, pos_set ? "ENTER to clear" : "-"};

    for (int i = 0; i < 3; ++i) {
        const int y = py + 40 + i * 26;
        if (i == selected) {
            fb_.fillRect(px + 6, y - 6, panel_w - 12, 16, COLOR_ADSB_SELECTED);
            drawSmallText(px + 10, y, ">", COLOR_BLACK);
        }
        drawSmallText(px + 18, y, labels[i],
                      i == selected ? COLOR_BLACK : COLOR_ADSB_TEXT);
        drawSmallText(px + 150, y, values[i],
                      i == selected ? COLOR_BLACK : COLOR_ADSB_SELECTED);
    }

    char src_line[32];
    snprintf(src_line, sizeof(src_line), "SOURCE: %s", source ? source : "-");
    drawSmallText(px + 18, py + 112, src_line, COLOR_TEXT_DIM);
    drawSmallText(px + 18, py + 128, "UP/DOWN SELECT  ENTER  H CLOSE",
                  COLOR_TEXT_DIM);
}
