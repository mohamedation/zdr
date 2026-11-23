#include "pages/rf433_page.h"

#include "app_config.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

Rf433Page::Rf433Page(Framebuffer& fb, SDRController& sdr, InputHandler& input,
                     Rf433UIRenderer& ui)
    : fb_(fb), sdr_(sdr), input_(input), ui_(ui) {}

Rf433Page::~Rf433Page() {
    controller_.stop();
}

bool Rf433Page::init() {
    std::cout << "Rf433Page: Initializing..." << std::endl;
    loadConfig();
    return true;
}

void Rf433Page::cleanup() {
    controller_.stop();
}

void Rf433Page::loadConfig() {
    frequency_mhz_ = 433.92f;
    gain_tenths_ = -1;
    filter_ = rf433::Filter::All;

    const auto& data = AppConfig::instance().data();
    if (!data.contains("rf433")) return;
    const auto& cfg = data["rf433"];
    frequency_mhz_ = cfg.value("frequency_mhz", frequency_mhz_);
    gain_tenths_ = cfg.value("gain_tenths", gain_tenths_);
    const std::string filter = cfg.value("filter", "all");
    if (filter == "tpms") filter_ = rf433::Filter::Tpms;
    else if (filter == "weather" || filter == "wx") {
        filter_ = rf433::Filter::Weather;
    } else if (filter == "other") filter_ = rf433::Filter::Other;
    else filter_ = rf433::Filter::All;
}

void Rf433Page::saveConfig() {
    auto& data = AppConfig::instance().data();
    if (!data.contains("rf433") || !data["rf433"].is_object()) {
        data["rf433"] = nlohmann::json::object();
    }
    auto& cfg = data["rf433"];
    cfg["frequency_mhz"] = frequency_mhz_;
    cfg["gain_tenths"] = gain_tenths_;
    switch (filter_) {
        case rf433::Filter::Tpms: cfg["filter"] = "tpms"; break;
        case rf433::Filter::Weather: cfg["filter"] = "weather"; break;
        case rf433::Filter::Other: cfg["filter"] = "other"; break;
        case rf433::Filter::All:
        default: cfg["filter"] = "all"; break;
    }
    AppConfig::instance().save();
}

void Rf433Page::startDecoder() {
    if (!controller_.start(frequency_mhz_, gain_tenths_)) {
        ui_.renderDialog("433", controller_.lastError().c_str(),
                         "R RETRY / TAB");
        fb_.update();
    }
}

void Rf433Page::restartDecoder() {
    controller_.stop();
    usleep(250000);
    startDecoder();
}

void Rf433Page::onEnter() {
    std::cout << "Rf433Page: Entering 433 mode" << std::endl;

    if (sdr_.isCapturing()) sdr_.stopCapture();
    sdr_.close();
    std::system("pkill -9 readsb 2>/dev/null");
    std::system("pkill -9 acarsdec 2>/dev/null");
    usleep(400000);

    loadConfig();
    view_ = View::List;
    selected_index_ = 0;
    scroll_offset_ = 0;
    help_visible_ = false;
    last_render_ = std::chrono::steady_clock::now();
    startDecoder();
    render();
}

void Rf433Page::onExit() {
    std::cout << "Rf433Page: Exiting 433 mode" << std::endl;
    controller_.stop();
}

void Rf433Page::update(float delta_time) {
    (void)delta_time;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_render_ < std::chrono::milliseconds(100)) return;
    last_render_ = now;
    render();
}

bool Rf433Page::handleInput(AppKeyCode key) {
    if (key == APPKEY_H) {
        help_visible_ = !help_visible_;
        return true;
    }
    if (help_visible_) {
        if (key == APPKEY_ESC) help_visible_ = false;
        return true;
    }

    const auto sensors = controller_.snapshotFiltered(filter_);
    const int count = static_cast<int>(sensors.size());

    switch (key) {
        case APPKEY_ENTER:
            if (view_ == View::List && count > 0) {
                selected_index_ =
                    std::max(0, std::min(selected_index_, count - 1));
                view_ = View::Detail;
            } else if (view_ == View::Detail) {
                view_ = View::List;
            }
            return true;

        case APPKEY_UP:
            if (view_ == View::List && selected_index_ > 0) {
                --selected_index_;
                if (selected_index_ < scroll_offset_) {
                    scroll_offset_ = selected_index_;
                }
            }
            return true;

        case APPKEY_DOWN:
            if (view_ == View::List && selected_index_ + 1 < count) {
                ++selected_index_;
                if (selected_index_ >= scroll_offset_ + 8) {
                    scroll_offset_ = selected_index_ - 7;
                }
            }
            return true;

        case APPKEY_T:
            filter_ = rf433::nextFilter(filter_);
            selected_index_ = 0;
            scroll_offset_ = 0;
            saveConfig();
            return true;

        case APPKEY_R:
            restartDecoder();
            return true;

        case APPKEY_ESC:
            if (view_ == View::Detail) {
                view_ = View::List;
                return true;
            }
            return false;

        default:
            return false;
    }
}

void Rf433Page::render() {
    const auto sensors = controller_.snapshotFiltered(filter_);
    if (view_ == View::Detail) {
        if (!sensors.empty()) {
            const int index = std::max(
                0, std::min(selected_index_,
                            static_cast<int>(sensors.size()) - 1));
            ui_.renderDetail(sensors[static_cast<size_t>(index)]);
            if (help_visible_) ui_.renderHelp();
            fb_.update();
            return;
        }
        view_ = View::List;
    }

    selected_index_ = sensors.empty()
                          ? 0
                          : std::min(selected_index_,
                                     static_cast<int>(sensors.size()) - 1);
    if (selected_index_ < scroll_offset_) scroll_offset_ = selected_index_;
    if (selected_index_ >= scroll_offset_ + 8) {
        scroll_offset_ = std::max(0, selected_index_ - 7);
    }

    ui_.renderList(sensors, selected_index_, scroll_offset_,
                   controller_.totalMessages(),
                   controller_.messagesPerSecond(), frequency_mhz_, filter_,
                   controller_.isRunning(), controller_.lastError());
    if (help_visible_) ui_.renderHelp();
    fb_.update();
}
