#ifndef RF433_PAGE_H
#define RF433_PAGE_H

#include "core/ipage.h"
#include "framebuffer.h"
#include "input_handler.h"
#include "rf433/rtl433_controller.h"
#include "sdr_controller.h"
#include "ui/rf433_ui_renderer.h"

#include <chrono>

class Rf433Page : public IPage {
public:
    Rf433Page(Framebuffer& fb, SDRController& sdr, InputHandler& input,
              Rf433UIRenderer& ui);
    ~Rf433Page() override;

    bool init() override;
    void cleanup() override;
    void onEnter() override;
    void onExit() override;
    void update(float delta_time) override;
    bool handleInput(AppKeyCode key) override;
    const char* getName() const override { return "433"; }
    const char* getDescription() const override {
        return "433 MHz Sensors";
    }

private:
    enum class View { List, Detail };

    void loadConfig();
    void saveConfig();
    void startDecoder();
    void restartDecoder();
    void render();

    Framebuffer& fb_;
    SDRController& sdr_;
    InputHandler& input_;
    Rf433UIRenderer& ui_;
    rf433::Rtl433Controller controller_;

    View view_ = View::List;
    int selected_index_ = 0;
    int scroll_offset_ = 0;
    bool help_visible_ = false;

    float frequency_mhz_ = 433.92f;
    int gain_tenths_ = -1;
    rf433::Filter filter_ = rf433::Filter::All;

    std::chrono::steady_clock::time_point last_render_{};
};

#endif  // RF433_PAGE_H
