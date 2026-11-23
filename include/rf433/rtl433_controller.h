#ifndef RTL433_CONTROLLER_H
#define RTL433_CONTROLLER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rf433 {

enum class Kind {
    Tpms,
    Weather,
    Other,
};

enum class Filter {
    All = 0,
    Tpms,
    Weather,
    Other,
};

inline const char* filterLabel(Filter f) {
    switch (f) {
        case Filter::Tpms: return "TPMS";
        case Filter::Weather: return "WX";
        case Filter::Other: return "OTHER";
        case Filter::All:
        default: return "ALL";
    }
}

inline Filter nextFilter(Filter f) {
    switch (f) {
        case Filter::All: return Filter::Tpms;
        case Filter::Tpms: return Filter::Weather;
        case Filter::Weather: return Filter::Other;
        case Filter::Other:
        default: return Filter::All;
    }
}

struct Sensor {
    std::string key;       // model|id
    std::string model;
    std::string id;
    std::string type;      // raw rtl_433 type string
    Kind kind = Kind::Other;
    float pressure_psi = -1.0f;     // <0 = unknown
    float temperature_c = -999.0f;  // <= -900 = unknown
    float humidity_pct = -1.0f;     // <0 = unknown
    float wind_kmh = -1.0f;         // <0 = unknown
    float rain_mm = -1.0f;          // <0 = unknown
    int battery_ok = -1;            // -1 unknown, 0 low, 1 ok
    double last_seen = 0.0;
    int hits = 0;
    std::string summary;            // short list value
};

/**
 * Runs rtl_433 as a child process and reads JSON lines from stdout.
 * Sensors are upserted by model+id so the list shows live state.
 */
class Rtl433Controller {
public:
    Rtl433Controller();
    ~Rtl433Controller();

    Rtl433Controller(const Rtl433Controller&) = delete;
    Rtl433Controller& operator=(const Rtl433Controller&) = delete;

    /**
     * @param frequency_mhz Center frequency (433.92 EU, 315 US common)
     * @param gain_tenths RTL gain in tenths of dB; <0 = auto
     */
    bool start(float frequency_mhz, int gain_tenths);
    void stop();
    bool isRunning() const { return running_.load(); }

    std::vector<Sensor> snapshot() const;
    std::vector<Sensor> snapshotFiltered(Filter filter) const;
    uint64_t totalMessages() const { return total_.load(); }
    float messagesPerSecond() const { return rate_.load(); }
    const std::string& lastError() const { return last_error_; }
    float frequencyMhz() const { return frequency_mhz_; }
    std::string decoderPath() const;

private:
    void readerLoop();
    void handleJson(const std::string& line);
    void noteExit(int status);
    static Kind classify(const Sensor& s);

    mutable std::mutex mutex_;
    std::vector<Sensor> sensors_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_{0};
    std::atomic<float> rate_{0.0f};
    pid_t pid_ = -1;
    int stdout_fd_ = -1;
    float frequency_mhz_ = 433.92f;
    int gain_tenths_ = -1;
    double started_at_ = 0.0;
    std::string last_error_;
};

}  // namespace rf433

#endif  // RTL433_CONTROLLER_H
