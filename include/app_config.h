#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <nlohmann/json.hpp>

#include <string>

// Single JSON configuration for the whole application:
//   $HOME/.config/zdr/config.json
//
// Sections:
//   system   - guide_show
//   sdr      - deemphasis_us, audio_agc, fft_size, sample_rate, theme
//   spectrum - configured, start_hz, stop_hz, max_hold_decay_db_s
//   adsb     - observer_latitude, observer_longitude, manual_override
//   imu      - enabled, heading_up, offset_deg, invert (radar compass)
//   map      - enabled, provider, dim, cache_hours, full_screen (radar basemap)
//   ais      - udp_export, udp_host, udp_port (NMEA !AIVDM feed)
//   acars    - channels[], gain, udp_port
//   rf433    - frequency_mhz, gain_tenths, filter (all|tpms|weather|other)
//
// On first load the legacy sdr_settings.json / spectrum_range.txt /
// adsb_config.json / guide.json files are imported once.
class AppConfig {
public:
    static AppConfig& instance();

    void load();
    bool save();

    nlohmann::json& data() { return data_; }
    const nlohmann::json& data() const { return data_; }

    const std::string& path() const { return path_; }

private:
    AppConfig();

    // Fill missing keys. Returns true if data_ was modified.
    bool setDefaults();
    void migrateLegacyFiles();

    nlohmann::json data_ = nlohmann::json::object();
    std::string path_;
};

#endif  // APP_CONFIG_H
