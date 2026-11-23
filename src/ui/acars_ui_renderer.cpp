#include "ui/acars_ui_renderer.h"

#include "ui/ui_text.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

std::string clockString(double timestamp) {
    time_t seconds = static_cast<time_t>(timestamp);
    if (seconds <= 0) seconds = time(nullptr);
    struct tm local {};
    localtime_r(&seconds, &local);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", local.tm_hour,
                  local.tm_min, local.tm_sec);
    return buffer;
}

std::vector<std::string> wrapText(const std::string& text, int columns) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
        if (static_cast<int>(current.size()) >= columns) {
            size_t cut = current.find_last_of(' ');
            if (cut != std::string::npos && cut > 10) {
                lines.push_back(current.substr(0, cut));
                current.erase(0, cut + 1);
            } else {
                lines.push_back(current);
                current.clear();
            }
        }
    }
    if (!current.empty()) lines.push_back(current);
    if (lines.empty()) lines.push_back("");
    return lines;
}

std::string firstLine(const std::string& text) {
    const size_t end = text.find('\n');
    std::string line = end == std::string::npos ? text : text.substr(0, end);
    if (line.size() > 18) line.resize(18);
    return line;
}

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

}  // namespace

AcarsUIRenderer::AcarsUIRenderer(Framebuffer& fb) : BaseUIRenderer(fb) {}

void AcarsUIRenderer::renderList(
    const std::vector<acars::Message>& messages, int selected_index,
    int scroll_offset, uint64_t total, float rate,
    const std::vector<float>& channels, float gain_db, bool running,
    const std::string& error) {
    fb_.clear(COLOR_BLACK);

    char title[40];
    if (running) {
        std::snprintf(title, sizeof(title), "ACARS  %.1f/s", rate);
    } else {
        std::snprintf(title, sizeof(title), "ACARS  OFF");
    }
    uiText().draw(fb_, 6, HEADER_Y + 2, title, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    char trk[32];
    std::snprintf(trk, sizeof(trk), "Msg:%llu",
                  static_cast<unsigned long long>(total));
    const int trk_w = uiText().width(trk, TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - trk_w, HEADER_Y + 2, trk, TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);
    uiText().draw(fb_, COL_TIME_X, COL_HEADER_Y + 1, "TIME", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_FLIGHT_X, COL_HEADER_Y + 1, "FLIGHT",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_TAIL_X, COL_HEADER_Y + 1, "TAIL", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_LBL_X, COL_HEADER_Y + 1, "LBL", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, COL_TEXT_X, COL_HEADER_Y + 1, "TEXT", TextStyle::Caption,
                  COLOR_ADSB_TEXT);

    if (messages.empty()) {
        const char* wait = error.empty() ? "WAITING FOR ACARS..." : error.c_str();
        const int w = uiText().width(wait, TextStyle::Caption);
        uiText().draw(fb_, (320 - w) / 2, 80, wait, TextStyle::Caption,
                      error.empty() ? COLOR_ADSB_TEXT : COLOR_ADSB_SELECTED);
    } else {
        const int visible = std::min(static_cast<int>(messages.size()) - scroll_offset,
                                     MAX_VISIBLE_ROWS);
        for (int i = 0; i < visible; ++i) {
            const int idx = scroll_offset + i;
            const auto& msg = messages[static_cast<size_t>(idx)];
            const int row_y = LIST_Y + i * ROW_HEIGHT;
            const bool selected = (idx == selected_index);
            if (selected) {
                fb_.fillRect(0, row_y, 320, ROW_HEIGHT, COLOR_ADSB_SELECTED);
            }
            const uint16_t color =
                selected ? COLOR_BLACK : COLOR_ADSB_TEXT;
            const int text_y = row_y + 2;

            char field[32];
            const std::string clock = clockString(msg.timestamp);
            uiText().draw(fb_, COL_TIME_X, text_y, clock.c_str(),
                          TextStyle::Caption, color);

            clipField(field, sizeof(field), msg.flight, 8);
            uiText().draw(fb_, COL_FLIGHT_X, text_y, field, TextStyle::Caption,
                          color);
            clipField(field, sizeof(field), msg.tail, 8);
            uiText().draw(fb_, COL_TAIL_X, text_y, field, TextStyle::Caption,
                          color);
            clipField(field, sizeof(field), msg.label, 4);
            uiText().draw(fb_, COL_LBL_X, text_y, field, TextStyle::Caption,
                          color);
            const std::string preview = firstLine(msg.text);
            uiText().draw(fb_, COL_TEXT_X, text_y, preview.c_str(),
                          TextStyle::Caption, color);
        }
    }

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    char bottom[64];
    if (!channels.empty()) {
        std::snprintf(bottom, sizeof(bottom), "%.3fM  G:%.0f  Enter Det  R Retry",
                      channels.front(), gain_db);
    } else {
        std::snprintf(bottom, sizeof(bottom), "G:%.0f  Enter Det  R Retry",
                      gain_db);
    }
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, bottom, TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}

void AcarsUIRenderer::renderDetail(const acars::Message& message, int scroll) {
    fb_.clear(COLOR_BLACK);

    uiText().draw(fb_, 6, HEADER_Y + 2, "ACARS DETAIL", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    const std::string clock = clockString(message.timestamp);
    const int cw = uiText().width(clock.c_str(), TextStyle::Caption);
    uiText().draw(fb_, 320 - 8 - cw, HEADER_Y + 2, clock.c_str(),
                  TextStyle::Caption, COLOR_ADSB_TEXT);

    drawHLine(2, COL_HEADER_Y + COL_HEADER_H - 1, 316, COLOR_RADAR_GRID);

    char line[48];
    int y = LIST_Y;
    auto drawRow = [&](const char* label, const char* value) {
        uiText().draw(fb_, 6, y, label, TextStyle::Caption, COLOR_ADSB_TEXT);
        uiText().draw(fb_, 70, y, value, TextStyle::Caption, COLOR_ADSB_TEXT);
        y += ROW_HEIGHT;
    };

    clipField(line, sizeof(line), message.flight, 12);
    drawRow("FLIGHT", line[0] ? line : "-");
    clipField(line, sizeof(line), message.tail, 12);
    drawRow("TAIL", line[0] ? line : "-");
    clipField(line, sizeof(line), message.label, 8);
    drawRow("LABEL", line[0] ? line : "-");
    std::snprintf(line, sizeof(line), "%.3f MHz", message.freq_mhz);
    drawRow("FREQ", line);

    const auto lines = wrapText(message.text.empty() ? "(no text)" : message.text,
                                48);
    const int max_lines = 5;
    const int start = std::max(0, std::min(scroll, static_cast<int>(lines.size()) - 1));
    for (int i = 0; i < max_lines && start + i < static_cast<int>(lines.size());
         ++i) {
        uiText().draw(fb_, 6, y + i * ROW_HEIGHT, lines[static_cast<size_t>(start + i)].c_str(),
                      TextStyle::Caption, COLOR_ADSB_TEXT);
    }

    drawHLine(2, STATS_BAR_Y, 316, COLOR_RADAR_GRID);
    uiText().draw(fb_, 6, STATS_BAR_Y + 3, "Esc List  Up/Dn Scroll",
                  TextStyle::Caption, COLOR_ADSB_TEXT);
}

void AcarsUIRenderer::renderHelp() {
    fb_.fillRect(24, 28, 272, 114, COLOR_BLACK);
    fb_.drawRect(24, 28, 272, 114, COLOR_RADAR_GRID);
    uiText().draw(fb_, 36, 36, "ACARS KEYS", TextStyle::Caption,
                  COLOR_ADSB_SELECTED);
    uiText().draw(fb_, 36, 54, "Enter  detail / back", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 68, "Up/Dn  select / scroll", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 82, "R      restart decoder", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 96, "H      this help", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
    uiText().draw(fb_, 36, 110, "M/Tab page menu", TextStyle::Caption,
                  COLOR_ADSB_TEXT);
}
