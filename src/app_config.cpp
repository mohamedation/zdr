#include "app_config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::string homeDir() {
    const char* home = std::getenv("HOME");
    return std::string(home != nullptr && *home != '\0' ? home : "/home/pi");
}

std::string configDir() { return homeDir() + "/.config/zdr"; }

nlohmann::json readJson(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return nlohmann::json();
    try {
        nlohmann::json value;
        in >> value;
        return value;
    } catch (...) {
        std::cerr << "AppConfig: cannot parse " << path << std::endl;
        return nlohmann::json();
    }
}

}  // namespace

AppConfig::AppConfig() : path_(configDir() + "/config.json") {}

AppConfig& AppConfig::instance() {
    static AppConfig config;
    return config;
}

void AppConfig::load() {
    data_ = nlohmann::json::object();

    std::error_code ec;
    const bool existed = std::filesystem::exists(path_, ec);
    if (existed) {
        const nlohmann::json loaded = readJson(path_);
        if (loaded.is_object()) {
            data_ = loaded;
        } else {
            std::cerr << "AppConfig: ignoring invalid " << path_
                      << ", using defaults" << std::endl;
        }
    } else {
        migrateLegacyFiles();
    }

    const bool added_defaults = setDefaults();
    std::cout << "AppConfig: " << (existed ? "loaded " : "created ")
              << path_ << std::endl;
    // Avoid rewriting the SD card on every launch; only persist when the file
    // is new or missing keys were filled in (upgrade / first run / migration).
    if (!existed || added_defaults) {
        save();
    }
}

bool AppConfig::setDefaults() {
    bool changed = false;

    auto section = [&](const char* name) -> nlohmann::json& {
        if (!data_.contains(name) || !data_[name].is_object()) {
            data_[name] = nlohmann::json::object();
            changed = true;
        }
        return data_[name];
    };

    auto set_if_missing = [&](nlohmann::json& obj, const char* key,
                              const nlohmann::json& value) {
        if (!obj.contains(key)) {
            obj[key] = value;
            changed = true;
        }
    };

    nlohmann::json& system = section("system");
    set_if_missing(system, "guide_show", true);

    nlohmann::json& sdr = section("sdr");
    set_if_missing(sdr, "deemphasis_us", 50);
    set_if_missing(sdr, "audio_agc", true);
    set_if_missing(sdr, "fft_size", 512);
    set_if_missing(sdr, "sample_rate", 1200000);
    set_if_missing(sdr, "theme", 0);

    nlohmann::json& spectrum = section("spectrum");
    set_if_missing(spectrum, "configured", false);
    set_if_missing(spectrum, "start_hz", 88000000);
    set_if_missing(spectrum, "stop_hz", 108000000);
    set_if_missing(spectrum, "max_hold_decay_db_s", 1.5);

    nlohmann::json& adsb = section("adsb");
    set_if_missing(adsb, "observer_latitude", 0.0);
    set_if_missing(adsb, "observer_longitude", 0.0);
    set_if_missing(adsb, "manual_override", false);

    nlohmann::json& imu = section("imu");
    set_if_missing(imu, "enabled", true);
    set_if_missing(imu, "heading_up", true);
    set_if_missing(imu, "offset_deg", 0.0);
    set_if_missing(imu, "invert", false);

    // Optional Web-Mercator basemap under ADS-B / AIS scopes.
    // full_screen=false keeps the map clipped to the radar circle so the
    // no-fill caption chrome stays readable.
    nlohmann::json& map = section("map");
    set_if_missing(map, "enabled", true);
    set_if_missing(map, "provider", "esri_dark");
    set_if_missing(map, "url_template", "");
    set_if_missing(map, "attribution", "");
    set_if_missing(map, "proxy", "");
    set_if_missing(map, "dim", 0.90);
    set_if_missing(map, "cache_hours", 168);
    set_if_missing(map, "full_screen", false);

    nlohmann::json& ais = section("ais");
    set_if_missing(ais, "udp_export", false);
    set_if_missing(ais, "udp_host", "255.255.255.255");
    set_if_missing(ais, "udp_port", 10110);

    nlohmann::json& acars = section("acars");
    if (!acars.contains("channels") || !acars["channels"].is_array()) {
        acars["channels"] = nlohmann::json::array({131.550, 131.725, 131.825});
        changed = true;
    }
    set_if_missing(acars, "gain", 40.0);
    set_if_missing(acars, "udp_port", 5556);

    nlohmann::json& rf433 = section("rf433");
    set_if_missing(rf433, "frequency_mhz", 433.92);
    set_if_missing(rf433, "gain_tenths", -1);
    set_if_missing(rf433, "filter", "all");  // all | tpms | weather | other

    return changed;
}

void AppConfig::migrateLegacyFiles() {
    const std::string dir = configDir();
    bool migrated = false;

    const nlohmann::json sdr = readJson(dir + "/sdr_settings.json");
    if (sdr.is_object()) {
        data_["sdr"]["deemphasis_us"] = sdr.value("deemphasis_us", 50);
        data_["sdr"]["audio_agc"] = sdr.value("audio_agc", true);
        data_["sdr"]["fft_size"] = sdr.value("fft_size", 512);
        data_["sdr"]["sample_rate"] = sdr.value("sample_rate", 1200000);
        data_["sdr"]["theme"] = sdr.value("theme", 0);
        migrated = true;
    }

    {
        std::ifstream in(dir + "/spectrum_range.txt");
        uint32_t start = 0;
        uint32_t stop = 0;
        if (in >> start >> stop && start >= 24000000 &&
            stop <= 1766000000 && stop > start + 500000) {
            data_["spectrum"]["configured"] = true;
            data_["spectrum"]["start_hz"] = start;
            data_["spectrum"]["stop_hz"] = stop;
            migrated = true;
        }
    }

    const nlohmann::json adsb = readJson(dir + "/adsb_config.json");
    if (adsb.is_object()) {
        data_["adsb"]["observer_latitude"] =
            adsb.value("observer_latitude", 0.0);
        data_["adsb"]["observer_longitude"] =
            adsb.value("observer_longitude", 0.0);
        data_["adsb"]["manual_override"] =
            adsb.value("manual_override", false);
        migrated = true;
    }

    const nlohmann::json guide = readJson(dir + "/guide.json");
    if (guide.is_object()) {
        data_["system"]["guide_show"] = guide.value("show", true);
        migrated = true;
    }

    if (migrated) {
        std::cout << "AppConfig: imported legacy settings from " << dir
                  << std::endl;
    }
}

bool AppConfig::save() {
    std::error_code ec;
    const std::filesystem::path target(path_);
    std::filesystem::create_directories(target.parent_path(), ec);

    const std::string temp = path_ + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "AppConfig: cannot write " << temp << std::endl;
            return false;
        }
        out << data_.dump(2) << "\n";
    }

    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::cerr << "AppConfig: cannot replace " << path_ << ": "
                  << ec.message() << std::endl;
        return false;
    }
    return true;
}
