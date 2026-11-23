#include "ui/ais_ui_renderer.h"

#include "ui/ui_text.h"
#include "ui/ship_glyph.h"
#ifndef ZDR_UI_PREVIEW
#include "adsb/map_background.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusNm = 3440.065;

const char* vesselMarker(const ais::VesselState& vessel) {
    if (vessel.aton) return "A";
    if (vessel.base_station) return "S";
    if (vessel.class_b) return "B";
    switch (vessel.ship_type) {
        case 30: return "F";
        case 31:
        case 32:
        case 52: return "U";
        case 36:
        case 37: return "Y";
        case 50: return "P";
        case 51: return "R";
        case 55: return "L";
        default: break;
    }
    if (vessel.ship_type >= 60 && vessel.ship_type < 70) return "P";
    if (vessel.ship_type >= 70 && vessel.ship_type < 80) return "C";
    if (vessel.ship_type >= 80 && vessel.ship_type < 90) return "T";
    return "?";
}

void formatAge(time_t now, time_t seen, char* out, size_t size) {
    const long age = static_cast<long>(now - seen);
    if (age < 60) {
        std::snprintf(out, size, "%2lds", age);
    } else if (age < 3600) {
        std::snprintf(out, size, "%2ldm", age / 60);
    } else {
        std::snprintf(out, size, "%2ldh", age / 3600);
    }
}

void drawCircleOutline(Framebuffer& fb, int cx, int cy, int radius,
                       uint16_t color) {
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        fb.drawPixel(cx + x, cy + y, color);
        fb.drawPixel(cx + y, cy + x, color);
        fb.drawPixel(cx - y, cy + x, color);
        fb.drawPixel(cx - x, cy + y, color);
        fb.drawPixel(cx - x, cy - y, color);
        fb.drawPixel(cx - y, cy - x, color);
        fb.drawPixel(cx + y, cy - x, color);
        fb.drawPixel(cx + x, cy - y, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

void drawDashedCircle(Framebuffer& fb, int cx, int cy, int radius,
                      uint16_t color) {
    for (int degrees = 0; degrees < 360; degrees += 8) {
        if ((degrees / 8) % 2 != 0) continue;
        const float a0 = degrees * static_cast<float>(kPi) / 180.0f;
        const float a1 = (degrees + 4) * static_cast<float>(kPi) / 180.0f;
        fb.drawLine(cx + static_cast<int>(std::lround(std::sin(a0) * radius)),
                    cy - static_cast<int>(std::lround(std::cos(a0) * radius)),
                    cx + static_cast<int>(std::lround(std::sin(a1) * radius)),
                    cy - static_cast<int>(std::lround(std::cos(a1) * radius)),
                    color);
    }
}

float distanceNm(double lat1, double lon1, double lat2, double lon2) {
    const double dlat = (lat2 - lat1) * kPi / 180.0;
    const double dlon = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                     std::cos(lat1 * kPi / 180.0) *
                         std::cos(lat2 * kPi / 180.0) *
                         std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    return static_cast<float>(2.0 * kEarthRadiusNm *
                              std::asin(std::min(1.0, std::sqrt(a))));
}

float bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    const double phi1 = lat1 * kPi / 180.0;
    const double phi2 = lat2 * kPi / 180.0;
    const double dlon = (lon2 - lon1) * kPi / 180.0;
    const double y = std::sin(dlon) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) -
                     std::sin(phi1) * std::cos(phi2) * std::cos(dlon);
    double degrees = std::atan2(y, x) * 180.0 / kPi;
    if (degrees < 0.0) degrees += 360.0;
    return static_cast<float>(degrees);
}

void drawTargetTriangle(Framebuffer& fb, int cx, int cy, float heading,
                        uint16_t color, int size) {
    const float angle = heading * static_cast<float>(kPi) / 180.0f;
    const int nose_x = cx + static_cast<int>(std::lround(std::sin(angle) * size));
    const int nose_y = cy - static_cast<int>(std::lround(std::cos(angle) * size));
    const int left_x =
        cx + static_cast<int>(std::lround(std::sin(angle + 2.4f) * size));
    const int left_y =
        cy - static_cast<int>(std::lround(std::cos(angle + 2.4f) * size));
    const int right_x =
        cx + static_cast<int>(std::lround(std::sin(angle - 2.4f) * size));
    const int right_y =
        cy - static_cast<int>(std::lround(std::cos(angle - 2.4f) * size));
    fb.drawLine(nose_x, nose_y, left_x, left_y, color);
    fb.drawLine(left_x, left_y, right_x, right_y, color);
    fb.drawLine(right_x, right_y, nose_x, nose_y, color);
}

const char* navStatusLabel(int status) {
    switch (status) {
        case 0: return "Under way";
        case 1: return "At anchor";
        case 2: return "Not under command";
        case 3: return "Restricted";
        case 4: return "Draft limited";
        case 5: return "Moored";
        case 6: return "Aground";
        case 7: return "Fishing";
        case 8: return "Sailing";
        case 15: return "Undefined";
        default: return "Unknown";
    }
}

}  // namespace

AisUIRenderer::AisUIRenderer(Framebuffer& fb) : BaseUIRenderer(fb) {}

void AisUIRenderer::drawChar(int x, int y, char c, uint16_t color) {
    if (c < 0x20 || c > 0x7E) return;
    char buf[2] = {c, '\0'};
    uiText().draw(fb_, x, y, buf, TextStyle::Mono, color);
}

void AisUIRenderer::drawText(int x, int y, const char* text, uint16_t color) {
    if (!text) return;
    uiText().draw(fb_, x, y, text, TextStyle::Mono, color);
}

void AisUIRenderer::drawSmallText(int x, int y, const char* text,
                                  uint16_t color) {
    if (!text) return;
    uiText().draw(fb_, x, y, text, TextStyle::Caption, color);
}

void AisUIRenderer::drawTextBg(int x, int y, const char* text, uint16_t fg,
                               uint16_t bg) {
    if (!text) return;
    const int w = getTextWidth(text);
    const int h = uiText().lineHeight(TextStyle::Mono);
    fb_.fillRect(x, y, w, h, bg);
    drawText(x, y, text, fg);
}

int AisUIRenderer::getTextWidth(const char* text) const {
    if (!text) return 0;
    return uiText().width(text, TextStyle::Mono);
}

int AisUIRenderer::smallTextWidth(const char* text) const {
    if (!text) return 0;
    return uiText().width(text, TextStyle::Caption);
}

void AisUIRenderer::renderListStatusBar(uint64_t total_messages,
                                        float messages_per_second,
                                        size_t vessels) {
    char title[40];
    std::snprintf(title, sizeof(title), "AIS  %.0f/s", messages_per_second);
    uiText().draw(fb_, 6, HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    char trk[40];
    if (udp_enabled_) {
        std::snprintf(trk, sizeof(trk), "Trk:%zu  Msg:%llu  UDP", vessels,
                      static_cast<unsigned long long>(total_messages));
    } else {
        std::snprintf(trk, sizeof(trk), "Trk:%zu  Msg:%llu", vessels,
                      static_cast<unsigned long long>(total_messages));
    }
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - trk_w, HEADER_Y + 2, trk, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderListFooter(uint64_t total_messages, size_t vessels,
                                     int gain_tenths, bool agc) {
    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    char stats[64];
    if (agc) {
        std::snprintf(stats, sizeof(stats), "Total:%llu  Vsl:%zu  AUTO",
                      static_cast<unsigned long long>(total_messages), vessels);
    } else {
        std::snprintf(stats, sizeof(stats), "Total:%llu  Vsl:%zu  %.0fdB",
                      static_cast<unsigned long long>(total_messages), vessels,
                      gain_tenths / 10.0f);
    }
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, stats, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    const char* hints = "Enter Plot  H Help";
    const int hw = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hw, STATS_BAR_Y + 3, hints, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderList(const std::vector<ais::VesselState>& vessels,
                               int selected_index, int scroll_offset,
                               uint64_t total_messages,
                               float messages_per_second, int gain_tenths,
                               bool agc) {
    fb_.clear(COLOR_BLACK);
    renderListStatusBar(total_messages, messages_per_second, vessels.size());

    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);
    drawText(COL_MMSI_X, COL_HEADER_Y + 1, "MMSI", COLOR_ADSB_TEXT);
    drawText(COL_NAME_X, COL_HEADER_Y + 1, "NAME", COLOR_ADSB_TEXT);
    drawText(COL_SOG_X, COL_HEADER_Y + 1, "SOG", COLOR_ADSB_TEXT);
    drawText(COL_COG_X, COL_HEADER_Y + 1, "COG", COLOR_ADSB_TEXT);
    drawText(COL_NM_X, COL_HEADER_Y + 1, "NM", COLOR_ADSB_TEXT);
    drawText(COL_AGE_X, COL_HEADER_Y + 1, "AGE", COLOR_ADSB_TEXT);

    const int count = static_cast<int>(vessels.size());
    if (count == 0) {
        const char* wait = "WAITING FOR AIS...";
        const int w = uiText().width(wait, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, wait, TextStyle::Caption,
                      COLOR_ADSB_TEXT);
    } else {
        const int first =
            std::max(0, std::min(scroll_offset, std::max(0, count - 1)));
        const int visible =
            std::min(count - first, MAX_VISIBLE_ROWS);
        const time_t now = time(nullptr);

        for (int row = 0; row < visible; ++row) {
            const int index = first + row;
            const ais::VesselState& vessel =
                vessels[static_cast<size_t>(index)];
            const int row_y = LIST_Y + row * ROW_HEIGHT;
            const bool selected = (index == selected_index);
            if (selected) {
                fb_.fillRect(0, row_y, 320, ROW_HEIGHT, COLOR_ADSB_SELECTED);
            }
            const uint16_t color =
                selected ? COLOR_BLACK : COLOR_ADSB_TEXT;
            const int text_y = row_y + 2;

            char buffer[24];
            std::snprintf(buffer, sizeof(buffer), "%09u", vessel.mmsi);
            drawText(COL_MMSI_X, text_y, buffer, color);

            char name[16];
            std::snprintf(name, sizeof(name), "%s%.9s", vesselMarker(vessel),
                          vessel.name.empty() ? "-----" : vessel.name.c_str());
            drawText(COL_NAME_X, text_y, name, color);

            if (vessel.sog >= 0.0f) {
                std::snprintf(buffer, sizeof(buffer), "%4.1f", vessel.sog);
            } else {
                std::snprintf(buffer, sizeof(buffer), "  -");
            }
            drawText(COL_SOG_X, text_y, buffer, color);

            if (vessel.cog >= 0.0f) {
                std::snprintf(buffer, sizeof(buffer), "%03.0f", vessel.cog);
            } else {
                std::snprintf(buffer, sizeof(buffer), "  -");
            }
            drawText(COL_COG_X, text_y, buffer, color);

            if (vessel.distance_nm >= 0.0f) {
                std::snprintf(buffer, sizeof(buffer), "%4.1f",
                              vessel.distance_nm);
            } else {
                std::snprintf(buffer, sizeof(buffer), "  -");
            }
            drawText(COL_NM_X, text_y, buffer, color);

            formatAge(now, vessel.last_seen, buffer, sizeof(buffer));
            drawText(COL_AGE_X, text_y, buffer, color);
        }
    }

    renderListFooter(total_messages, vessels.size(), gain_tenths, agc);
}

void AisUIRenderer::renderConsole(
    const std::vector<ais::AisLogEntry>& entries, int scroll_offset,
    uint64_t total_messages, float messages_per_second, int gain_tenths,
    bool agc, size_t vessels) {
    (void)gain_tenths;
    (void)agc;
    fb_.clear(COLOR_BLACK);
    renderListStatusBar(total_messages, messages_per_second, vessels);
    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);

    constexpr int kVisibleLines = 8;
    const int total = static_cast<int>(entries.size());
    const int end = std::max(0, total - scroll_offset);
    const int start = std::max(0, end - kVisibleLines);
    const int lines = end - start;

    if (total == 0) {
        const char* wait = "WAITING FOR AIS...";
        const int w = uiText().width(wait, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, wait, TextStyle::Caption,
                      COLOR_ADSB_TEXT);
    } else {
        for (int i = 0; i < lines; ++i) {
            const int index = start + i;
            const int y = LIST_Y + i * ROW_HEIGHT;
            const bool newest = (index == total - 1);
            uiText().draw(fb_, 6, y + 2,
                          entries[static_cast<size_t>(index)].text.c_str(),
                          TextStyle::Caption,
                          newest ? COLOR_ADSB_SELECTED : COLOR_ADSB_TEXT);
        }
    }

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, "162.000 1.92M", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    const char* hints = "Enter List  H Help";
    const int hw = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hw, STATS_BAR_Y + 3, hints, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderPlotStatusBar(float range_nm, size_t vessels,
                                        uint64_t total_messages) {
    char title[32];
    std::snprintf(title, sizeof(title), "AIS  %.0fNM", range_nm);
    uiText().draw(fb_, 6, SCOPE_HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    char trk[40];
    std::snprintf(trk, sizeof(trk), "Trk:%zu  Msg:%llu", vessels,
                  static_cast<unsigned long long>(total_messages));
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - trk_w, SCOPE_HEADER_Y + 2, trk,
                  TextStyle::Caption, COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderPlotFooter(const ais::VesselState* selected,
                                     float range_nm) {
    (void)range_nm;
    drawHLine(2, SCOPE_BOTTOM_Y, 316, COLOR_RADAR_GRID);

    char name[20] = "---";
    char info[32] = {0};
    if (selected != nullptr) {
        if (!selected->name.empty()) {
            std::snprintf(name, sizeof(name), "%.12s", selected->name.c_str());
        } else {
            std::snprintf(name, sizeof(name), "%09u", selected->mmsi);
        }
        if (selected->distance_nm >= 0.0f) {
            std::snprintf(info, sizeof(info), "%.1fNM %03.0f",
                          selected->distance_nm, selected->bearing_deg);
        }
    }

    const uint16_t name_color =
        selected != nullptr ? COLOR_ADSB_SELECTED : COLOR_ADSB_TEXT;
    uiText().draw(fb_, 6, SCOPE_BOTTOM_Y + 3, name, TextStyle::Caption,
                  name_color);
    if (info[0] != '\0') {
        const int nw = uiText().width(name, TextStyle::Caption);
        uiText().draw(fb_, 6 + nw + 8, SCOPE_BOTTOM_Y + 3, info,
                      TextStyle::Caption, COLOR_ADSB_TEXT);
    }

    const char* hints = "Up/Dn Rng  <> Tgt";
    const int hw = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hw, SCOPE_BOTTOM_Y + 3, hints,
                  TextStyle::Caption, COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderPlot(
    const std::vector<ais::VesselState>& vessels, int selected_index,
    float range_nm, bool station_valid, double station_latitude,
    double station_longitude, uint64_t total_messages,
    float messages_per_second, int gain_tenths, bool agc,
    MapBackground* map) {
    (void)messages_per_second;
    (void)gain_tenths;
    (void)agc;

    fb_.clear(COLOR_BLACK);

    const int cx = SCOPE_CENTER_X;
    const int cy = SCOPE_CENTER_Y;
    const int radius = SCOPE_RADIUS;

#ifdef ZDR_UI_PREVIEW
    (void)map;
    const bool has_map = false;
#else
    const bool has_map =
        (map != nullptr && map->hasImage() && station_valid);
    if (has_map) {
        map->draw(fb_, cx, cy, radius, range_nm * 1.852f, 0.0f);
    }
#endif

    renderPlotStatusBar(range_nm, vessels.size(), total_messages);

    for (int ring = 1; ring <= 3; ++ring) {
        drawCircleOutline(fb_, cx, cy, radius * ring / 3, COLOR_RADAR_GRID);
    }

    if (range_nm >= 0.5f) {
        const int guard_radius = std::max(
            1, static_cast<int>(std::lround(0.5f / range_nm * radius)));
        drawDashedCircle(fb_, cx, cy, guard_radius, COLOR_ADSB_SELECTED);
    }

    fb_.drawLine(cx - radius, cy, cx + radius, cy, COLOR_RADAR_GRID);
    fb_.drawLine(cx, cy - radius, cx, cy + radius, COLOR_RADAR_GRID);

    for (int degrees = 0; degrees < 360; degrees += 30) {
        const float angle = degrees * static_cast<float>(kPi) / 180.0f;
        const int inner = (degrees % 90 == 0) ? radius - 6 : radius - 3;
        const int x1 = cx + static_cast<int>(std::lround(std::sin(angle) * inner));
        const int y1 = cy - static_cast<int>(std::lround(std::cos(angle) * inner));
        const int x2 =
            cx + static_cast<int>(std::lround(std::sin(angle) * radius));
        const int y2 =
            cy - static_cast<int>(std::lround(std::cos(angle) * radius));
        fb_.drawLine(x1, y1, x2, y2, COLOR_RADAR_GRID);
    }

    drawSmallText(cx - 3, cy - radius + 2, "N", COLOR_ADSB_TEXT);
    drawSmallText(cx + radius - 8, cy - 3, "E", COLOR_ADSB_TEXT);
    drawSmallText(cx - 3, cy + radius - 10, "S", COLOR_ADSB_TEXT);
    drawSmallText(cx - radius + 2, cy - 3, "W", COLOR_ADSB_TEXT);

    // Own ship marker.
    fb_.drawLine(cx, cy - 5, cx - 3, cy + 3, COLOR_ADSB_TEXT);
    fb_.drawLine(cx, cy - 5, cx + 3, cy + 3, COLOR_ADSB_TEXT);
    fb_.drawLine(cx - 3, cy + 3, cx + 3, cy + 3, COLOR_ADSB_TEXT);
    fb_.drawPixel(cx, cy, COLOR_ADSB_TEXT);

    int drawn = 0;
    for (int index = 0; index < static_cast<int>(vessels.size()); ++index) {
        const ais::VesselState& vessel = vessels[static_cast<size_t>(index)];
        if (!vessel.has_position || vessel.distance_nm < 0.0f) continue;
        if (vessel.distance_nm > range_nm * 1.05f) continue;

        const int x = cx + static_cast<int>(std::lround(
            std::sin(vessel.bearing_deg * kPi / 180.0) *
            (vessel.distance_nm / range_nm) * radius));
        const int y = cy - static_cast<int>(std::lround(
            std::cos(vessel.bearing_deg * kPi / 180.0) *
            (vessel.distance_nm / range_nm) * radius));

        for (size_t t = 0; t < vessel.trail.size(); ++t) {
            const float distance = distanceNm(
                station_latitude, station_longitude, vessel.trail[t].first,
                vessel.trail[t].second);
            if (distance > range_nm) continue;
            const float bearing = bearingDeg(
                station_latitude, station_longitude, vessel.trail[t].first,
                vessel.trail[t].second);
            const int tx = cx + static_cast<int>(std::lround(
                std::sin(bearing * kPi / 180.0) * (distance / range_nm) *
                radius));
            const int ty = cy - static_cast<int>(std::lround(
                std::cos(bearing * kPi / 180.0) * (distance / range_nm) *
                radius));
            fb_.drawPixel(tx, ty, COLOR_RADAR_GRID);
        }

        const bool selected = (index == selected_index);
        const bool alarm = !vessel.stale && vessel.cpa_nm >= 0.0f &&
                           vessel.cpa_nm < 0.5f && vessel.tcpa_min >= 0.0f &&
                           vessel.tcpa_min < 10.0f;
        const uint16_t color =
            selected ? COLOR_ADSB_SELECTED
                     : (alarm ? static_cast<uint16_t>(0xF800) : COLOR_ADSB_TEXT);

        if (vessel.cog >= 0.0f && vessel.sog > 0.3f) {
            const float length = 7.0f + std::min(13.0f, vessel.sog * 1.2f);
            const float course = vessel.cog * static_cast<float>(kPi) / 180.0f;
            const int ex =
                x + static_cast<int>(std::lround(std::sin(course) * length));
            const int ey =
                y - static_cast<int>(std::lround(std::cos(course) * length));
            fb_.drawLine(x, y, ex, ey, color);
        }

        if (vessel.base_station) {
            fb_.drawRect(x - 2, y - 2, 5, 5, color);
        } else if (vessel.aton) {
            drawCircleOutline(fb_, x, y, 2, color);
        } else if (vessel.class_b) {
            fb_.drawLine(x, y - 4, x + 4, y, color);
            fb_.drawLine(x + 4, y, x, y + 4, color);
            fb_.drawLine(x, y + 4, x - 4, y, color);
            fb_.drawLine(x - 4, y, x, y - 4, color);
        } else {
            drawTargetTriangle(fb_, x, y,
                               vessel.cog >= 0.0f ? vessel.cog
                                                  : vessel.bearing_deg,
                               color, selected ? 5 : 4);
        }
        fb_.drawPixel(x, y, color);

        if (selected) {
            constexpr int r = 7;
            fb_.drawLine(x - r, y - r, x - r + 3, y - r, COLOR_ADSB_SELECTED);
            fb_.drawLine(x - r, y - r, x - r, y - r + 3, COLOR_ADSB_SELECTED);
            fb_.drawLine(x + r, y - r, x + r - 3, y - r, COLOR_ADSB_SELECTED);
            fb_.drawLine(x + r, y - r, x + r, y - r + 3, COLOR_ADSB_SELECTED);
            fb_.drawLine(x - r, y + r, x - r + 3, y + r, COLOR_ADSB_SELECTED);
            fb_.drawLine(x - r, y + r, x - r, y + r - 3, COLOR_ADSB_SELECTED);
            fb_.drawLine(x + r, y + r, x + r - 3, y + r, COLOR_ADSB_SELECTED);
            fb_.drawLine(x + r, y + r, x + r, y + r - 3, COLOR_ADSB_SELECTED);
        }
        ++drawn;
    }

    if (!station_valid) {
        const char* msg = "POSITION NOT SET";
        const int w = uiText().width(msg, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, msg, TextStyle::Caption,
                      COLOR_ADSB_SELECTED);
    } else if (drawn == 0) {
        const char* msg = "WAITING FOR AIS";
        const int w = uiText().width(msg, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, msg, TextStyle::Caption,
                      COLOR_ADSB_TEXT);
    }

    const ais::VesselState* selected = nullptr;
    if (selected_index >= 0 &&
        selected_index < static_cast<int>(vessels.size())) {
        selected = &vessels[static_cast<size_t>(selected_index)];
    }
    renderPlotFooter(selected, range_nm);
    (void)has_map;
}

void AisUIRenderer::renderDossier(const ais::VesselState* vessel,
                                  size_t vessel_count,
                                  uint64_t total_messages,
                                  float messages_per_second,
                                  int gain_tenths, bool agc) {
    (void)total_messages;
    (void)messages_per_second;
    (void)gain_tenths;
    (void)agc;

    fb_.clear(COLOR_BLACK);

    constexpr int kSplitX = 168;
    constexpr int kRoseCx = 84;
    constexpr int kRoseCy = 100;
    constexpr int kRoseR = 48;

    if (vessel == nullptr) {
        uiText().draw(fb_, 8, 6, "NO VESSEL", TextStyle::Title,
                      COLOR_ADSB_SELECTED);
        uiText().draw(fb_, 8, 28, "----", TextStyle::Caption, COLOR_ADSB_TEXT);
        drawVLine(kSplitX, 4, 146, COLOR_ADSB_TEXT);
        drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
        char trk[32];
        std::snprintf(trk, sizeof(trk), "AIS %zu", vessel_count);
        uiText().draw(fb_, 6, STATS_BAR_Y + 3, trk, TextStyle::Caption,
                      COLOR_ADSB_TEXT);
        return;
    }

    char name[20];
    if (!vessel->name.empty()) {
        std::snprintf(name, sizeof(name), "%.12s", vessel->name.c_str());
    } else {
        std::snprintf(name, sizeof(name), "%09u", vessel->mmsi);
    }
    uiText().draw(fb_, 8, 6, name, TextStyle::Title, COLOR_ADSB_SELECTED);

    const char* type = ais::AisStore::shipTypeLabel(vessel->ship_type);
    char subtype[24];
    if (vessel->base_station) {
        std::snprintf(subtype, sizeof(subtype), "BASE");
    } else if (vessel->aton) {
        std::snprintf(subtype, sizeof(subtype), "ATON");
    } else if (type[0] != '\0') {
        std::snprintf(subtype, sizeof(subtype), "%s", type);
    } else {
        std::snprintf(subtype, sizeof(subtype), "%s",
                      vessel->class_b ? "CLASS B" : "CLASS A");
    }
    uiText().draw(fb_, 8, 28, subtype, TextStyle::Caption, COLOR_ADSB_TEXT);

    for (int deg = 0; deg < 360; deg += 10) {
        if ((deg / 10) % 2 != 0) continue;
        const float a0 = deg * static_cast<float>(kPi) / 180.0f;
        const float a1 = (deg + 6) * static_cast<float>(kPi) / 180.0f;
        fb_.drawLine(
            kRoseCx + static_cast<int>(std::lround(std::sin(a0) * kRoseR)),
            kRoseCy - static_cast<int>(std::lround(std::cos(a0) * kRoseR)),
            kRoseCx + static_cast<int>(std::lround(std::sin(a1) * kRoseR)),
            kRoseCy - static_cast<int>(std::lround(std::cos(a1) * kRoseR)),
            COLOR_ADSB_TEXT);
    }
    uiText().draw(fb_, kRoseCx - 3, kRoseCy - kRoseR + 2, "N",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
    uiText().draw(fb_, kRoseCx + kRoseR - 10, kRoseCy - 4, "E",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
    uiText().draw(fb_, kRoseCx - 3, kRoseCy + kRoseR - 12, "S",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
    uiText().draw(fb_, kRoseCx - kRoseR + 2, kRoseCy - 4, "W",
                  TextStyle::Caption, COLOR_ADSB_TEXT);

    float hdg = 0.0f;
    if (vessel->heading >= 0) hdg = static_cast<float>(vessel->heading);
    else if (vessel->cog >= 0.0f) hdg = vessel->cog;
    ShipGlyph::draw(fb_, kRoseCx, kRoseCy, COLOR_ADSB_SELECTED, hdg, 2.0f);

    drawVLine(kSplitX, 4, 146, COLOR_ADSB_TEXT);

    auto row = [&](int y, const char* label, const char* value) {
        uiText().draw(fb_, kSplitX + 10, y, label, TextStyle::Mono,
                      COLOR_ADSB_TEXT);
        uiText().draw(fb_, kSplitX + 58, y, value, TextStyle::Mono,
                      COLOR_ADSB_TEXT);
    };

    char value[32];
    int y = 10;
    constexpr int kRow = 18;

    std::snprintf(value, sizeof(value), "%09u", vessel->mmsi);
    row(y, "MMSI", value);
    y += kRow;

    if (vessel->sog >= 0.0f) {
        std::snprintf(value, sizeof(value), "%.1f KN", vessel->sog);
    } else {
        std::snprintf(value, sizeof(value), "---");
    }
    row(y, "SOG", value);
    y += kRow;

    if (vessel->cog >= 0.0f) {
        std::snprintf(value, sizeof(value), "%03.0f DEG", vessel->cog);
    } else {
        std::snprintf(value, sizeof(value), "---");
    }
    row(y, "COG", value);
    y += kRow;

    if (vessel->distance_nm >= 0.0f) {
        std::snprintf(value, sizeof(value), "%.1f NM", vessel->distance_nm);
    } else {
        std::snprintf(value, sizeof(value), "---");
    }
    row(y, "RNG", value);
    y += kRow;

    if (vessel->distance_nm >= 0.0f) {
        std::snprintf(value, sizeof(value), "%03.0f DEG", vessel->bearing_deg);
    } else {
        std::snprintf(value, sizeof(value), "---");
    }
    row(y, "BRG", value);
    y += kRow;

    if (vessel->cpa_nm >= 0.0f) {
        if (vessel->tcpa_min >= 0.0f) {
            std::snprintf(value, sizeof(value), "%.1f / %.0fm", vessel->cpa_nm,
                          vessel->tcpa_min);
        } else {
            std::snprintf(value, sizeof(value), "%.1f NM", vessel->cpa_nm);
        }
    } else {
        std::snprintf(value, sizeof(value), "---");
    }
    row(y, "CPA", value);
    y += kRow;

    if (!vessel->destination.empty()) {
        std::snprintf(value, sizeof(value), "%.10s",
                      vessel->destination.c_str());
    } else if (!vessel->callsign.empty()) {
        std::snprintf(value, sizeof(value), "%.7s", vessel->callsign.c_str());
    } else {
        std::snprintf(value, sizeof(value), "----");
    }
    row(y, vessel->destination.empty() ? "CALL" : "DEST", value);

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    char footer[40];
    if (udp_enabled_) {
        std::snprintf(footer, sizeof(footer), "AIS %zu  UDP", vessel_count);
    } else {
        std::snprintf(footer, sizeof(footer), "AIS %zu", vessel_count);
    }
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, footer, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    const char* hints = "Enter Console  <> Tgt";
    const int hw = uiText().width(hints, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - hw, STATS_BAR_Y + 3, hints, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void AisUIRenderer::renderHelp() {
    fb_.fillRect(24, 28, 272, 114, COLOR_BLACK);
    fb_.drawRect(24, 28, 272, 114, COLOR_RADAR_GRID);
    uiText().draw(fb_, 36, 36, "AIS KEYS", TextStyle::Caption,
                  COLOR_ADSB_SELECTED);
    const char* lines[] = {
        "Enter  list / plot / dossier / console",
        "Up/Dn  scroll or plot range",
        "Left/Right  plot target",
        "+/- G  gain / AGC",
        "U      UDP NMEA 10110",
        "H      this help",
    };
    for (int i = 0; i < 6; ++i) {
        uiText().draw(fb_, 36, 54 + i * 12, lines[i], TextStyle::Caption,
                      COLOR_ADSB_TEXT);
    }
}
