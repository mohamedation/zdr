#include "rf433/rtl433_controller.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace rf433 {
namespace {

constexpr size_t kMaxSensors = 80;

double nowSeconds() {
    struct timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;
}

bool isExecutable(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR) != 0;
}

float asFloat(const nlohmann::json& doc, const char* key, float fallback) {
    const auto it = doc.find(key);
    if (it == doc.end() || it->is_null()) return fallback;
    if (it->is_number()) return static_cast<float>(it->get<double>());
    if (it->is_string()) {
        try {
            return std::stof(it->get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

std::string asString(const nlohmann::json& doc, const char* key) {
    const auto it = doc.find(key);
    if (it == doc.end() || it->is_null()) return {};
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number_integer()) return std::to_string(it->get<long long>());
    if (it->is_number()) return std::to_string(it->get<double>());
    if (it->is_boolean()) return it->get<bool>() ? "1" : "0";
    return {};
}

bool containsIgnoreCase(const std::string& hay, const char* needle) {
    auto lower = [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    };
    std::string h = hay;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), lower);
    std::transform(n.begin(), n.end(), n.begin(), lower);
    return h.find(n) != std::string::npos;
}

}  // namespace

Rtl433Controller::Rtl433Controller() = default;

Rtl433Controller::~Rtl433Controller() {
    stop();
}

std::string Rtl433Controller::decoderPath() const {
    const char* candidates[] = {
        "/usr/bin/rtl_433",
        "/usr/local/bin/rtl_433",
    };
    for (const char* path : candidates) {
        if (isExecutable(path)) return path;
    }
    return "rtl_433";
}

Kind Rtl433Controller::classify(const Sensor& s) {
    if (containsIgnoreCase(s.type, "TPMS") ||
        containsIgnoreCase(s.model, "TPMS") || s.pressure_psi >= 0.0f) {
        return Kind::Tpms;
    }
    if (s.humidity_pct >= 0.0f || s.wind_kmh >= 0.0f || s.rain_mm >= 0.0f ||
        containsIgnoreCase(s.type, "Weather") ||
        containsIgnoreCase(s.model, "Weather") ||
        containsIgnoreCase(s.model, "Thermo") ||
        containsIgnoreCase(s.model, "Acurite") ||
        containsIgnoreCase(s.model, "Bresser") ||
        containsIgnoreCase(s.model, "Oregon") ||
        containsIgnoreCase(s.model, "LaCrosse") ||
        (s.temperature_c > -900.0f && s.pressure_psi < 0.0f)) {
        // Temperature-only remotes without tire pressure count as weather.
        if (s.pressure_psi < 0.0f) return Kind::Weather;
    }
    return Kind::Other;
}

bool Rtl433Controller::start(float frequency_mhz, int gain_tenths) {
    stop();

    frequency_mhz_ = frequency_mhz > 0.0f ? frequency_mhz : 433.92f;
    gain_tenths_ = gain_tenths;

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        last_error_ = std::string("pipe: ") + std::strerror(errno);
        return false;
    }

    const std::string path = decoderPath();
    std::vector<std::string> args;
    args.push_back(path);
    args.push_back("-f");
    char freq[32];
    std::snprintf(freq, sizeof(freq), "%.0f", frequency_mhz_ * 1e6f);
    args.push_back(freq);
    args.push_back("-F");
    args.push_back("json");
    args.push_back("-M");
    args.push_back("time:unix");
    if (gain_tenths_ >= 0) {
        char gain[16];
        std::snprintf(gain, sizeof(gain), "%d", gain_tenths_);
        args.push_back("-g");
        args.push_back(gain);
    }

    std::cout << "Rtl433Controller:";
    for (const auto& a : args) std::cout << ' ' << a;
    std::cout << std::endl;

    pid_ = fork();
    if (pid_ < 0) {
        last_error_ = std::string("fork: ") + std::strerror(errno);
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    if (pid_ == 0) {
        setsid();
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int logfd = open("/tmp/zdr-rtl433.log",
                         O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        close(pipefd[1]);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execv(args[0].c_str(), argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    stdout_fd_ = pipefd[0];
    fcntl(stdout_fd_, F_SETFL, O_NONBLOCK);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sensors_.clear();
        last_error_.clear();
    }
    total_.store(0);
    rate_.store(0.0f);
    started_at_ = nowSeconds();
    running_.store(true);
    thread_ = std::thread(&Rtl433Controller::readerLoop, this);
    std::cout << "Rtl433Controller: rtl_433 pid " << pid_ << " @ "
              << frequency_mhz_ << " MHz" << std::endl;
    return true;
}

void Rtl433Controller::stop() {
    if (!running_.exchange(false) && pid_ <= 0) {
        if (stdout_fd_ >= 0) {
            close(stdout_fd_);
            stdout_fd_ = -1;
        }
        return;
    }
    if (thread_.joinable()) thread_.join();

    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        bool reaped = false;
        for (int i = 0; i < 30; ++i) {
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                reaped = true;
                break;
            }
            usleep(50000);
        }
        if (!reaped) {
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, 0);
        }
        pid_ = -1;
    }
    if (stdout_fd_ >= 0) {
        close(stdout_fd_);
        stdout_fd_ = -1;
    }
}

void Rtl433Controller::readerLoop() {
    std::string pending;
    char buffer[4096];
    while (running_.load()) {
        const ssize_t n = read(stdout_fd_, buffer, sizeof(buffer));
        if (n > 0) {
            pending.append(buffer, static_cast<size_t>(n));
            size_t start = 0;
            while (true) {
                const size_t end = pending.find('\n', start);
                if (end == std::string::npos) {
                    pending.erase(0, start);
                    break;
                }
                const std::string line = pending.substr(start, end - start);
                if (!line.empty() && line[0] == '{') handleJson(line);
                start = end + 1;
            }
        } else if (n == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            break;
        } else {
            usleep(20000);
        }

        if (pid_ > 0) {
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                noteExit(status);
                break;
            }
        }
        const double elapsed = std::max(0.001, nowSeconds() - started_at_);
        rate_.store(static_cast<float>(total_.load() / elapsed));
    }
}

void Rtl433Controller::noteExit(int status) {
    pid_ = -1;
    running_.store(false);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        last_error_ = "cannot execute rtl_433";
    } else {
        last_error_ = "rtl_433 stopped";
    }
    std::cerr << "Rtl433Controller: " << last_error_ << std::endl;
}

void Rtl433Controller::handleJson(const std::string& line) {
    nlohmann::json doc = nlohmann::json::parse(line, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) return;

    Sensor s;
    s.model = asString(doc, "model");
    s.id = asString(doc, "id");
    if (s.id.empty()) s.id = asString(doc, "sensor_id");
    if (s.model.empty() && s.id.empty()) return;

    s.key = s.model + "|" + s.id;
    s.type = asString(doc, "type");

    s.pressure_psi = asFloat(doc, "pressure_PSI", -1.0f);
    if (s.pressure_psi < 0.0f) {
        const float kpa = asFloat(doc, "pressure_kPa", -1.0f);
        if (kpa >= 0.0f) s.pressure_psi = kpa * 0.145038f;
    }
    if (s.pressure_psi < 0.0f) {
        const float bar = asFloat(doc, "pressure_bar", -1.0f);
        if (bar >= 0.0f) s.pressure_psi = bar * 14.5038f;
    }

    s.temperature_c = asFloat(doc, "temperature_C", -999.0f);
    if (s.temperature_c <= -900.0f) {
        const float f = asFloat(doc, "temperature_F", -999.0f);
        if (f > -900.0f) s.temperature_c = (f - 32.0f) * (5.0f / 9.0f);
    }

    s.humidity_pct = asFloat(doc, "humidity", -1.0f);
    if (s.humidity_pct < 0.0f) {
        s.humidity_pct = asFloat(doc, "humidity_pct", -1.0f);
    }

    s.wind_kmh = asFloat(doc, "wind_avg_km_h", -1.0f);
    if (s.wind_kmh < 0.0f) s.wind_kmh = asFloat(doc, "wind_speed_km_h", -1.0f);
    if (s.wind_kmh < 0.0f) {
        const float ms = asFloat(doc, "wind_avg_m_s", -1.0f);
        if (ms >= 0.0f) s.wind_kmh = ms * 3.6f;
    }

    s.rain_mm = asFloat(doc, "rain_mm", -1.0f);
    if (s.rain_mm < 0.0f) s.rain_mm = asFloat(doc, "rain_rate", -1.0f);

    if (doc.contains("battery_ok")) {
        const auto& b = doc["battery_ok"];
        if (b.is_boolean()) s.battery_ok = b.get<bool>() ? 1 : 0;
        else if (b.is_number()) s.battery_ok = b.get<int>() != 0 ? 1 : 0;
    }

    s.last_seen = asFloat(doc, "time", 0.0f);
    if (s.last_seen <= 0.0) s.last_seen = nowSeconds();

    s.kind = classify(s);

    char summary[48];
    if (s.kind == Kind::Tpms) {
        if (s.pressure_psi >= 0.0f && s.temperature_c > -900.0f) {
            std::snprintf(summary, sizeof(summary), "%.1fpsi %.0fC",
                          s.pressure_psi, s.temperature_c);
        } else if (s.pressure_psi >= 0.0f) {
            std::snprintf(summary, sizeof(summary), "%.1fpsi", s.pressure_psi);
        } else if (s.temperature_c > -900.0f) {
            std::snprintf(summary, sizeof(summary), "%.0fC", s.temperature_c);
        } else {
            std::snprintf(summary, sizeof(summary), "TPMS");
        }
    } else if (s.kind == Kind::Weather) {
        if (s.temperature_c > -900.0f && s.humidity_pct >= 0.0f) {
            std::snprintf(summary, sizeof(summary), "%.0fC %.0f%%",
                          s.temperature_c, s.humidity_pct);
        } else if (s.temperature_c > -900.0f) {
            std::snprintf(summary, sizeof(summary), "%.1fC", s.temperature_c);
        } else if (s.humidity_pct >= 0.0f) {
            std::snprintf(summary, sizeof(summary), "%.0f%%RH", s.humidity_pct);
        } else if (s.wind_kmh >= 0.0f) {
            std::snprintf(summary, sizeof(summary), "%.0fkm/h", s.wind_kmh);
        } else if (s.rain_mm >= 0.0f) {
            std::snprintf(summary, sizeof(summary), "%.1fmm", s.rain_mm);
        } else {
            std::snprintf(summary, sizeof(summary), "WX");
        }
    } else {
        if (s.temperature_c > -900.0f) {
            std::snprintf(summary, sizeof(summary), "%.1fC", s.temperature_c);
        } else {
            const std::string channel = asString(doc, "channel");
            std::snprintf(summary, sizeof(summary), "%s",
                          channel.empty() ? "-" : channel.c_str());
        }
    }
    s.summary = summary;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(sensors_.begin(), sensors_.end(),
                               [&](const Sensor& x) { return x.key == s.key; });
        if (it != sensors_.end()) {
            s.hits = it->hits + 1;
            if (s.pressure_psi < 0.0f) s.pressure_psi = it->pressure_psi;
            if (s.temperature_c <= -900.0f) s.temperature_c = it->temperature_c;
            if (s.humidity_pct < 0.0f) s.humidity_pct = it->humidity_pct;
            if (s.wind_kmh < 0.0f) s.wind_kmh = it->wind_kmh;
            if (s.rain_mm < 0.0f) s.rain_mm = it->rain_mm;
            if (s.battery_ok < 0) s.battery_ok = it->battery_ok;
            s.kind = classify(s);
            *it = s;
        } else {
            s.hits = 1;
            sensors_.push_back(std::move(s));
            while (sensors_.size() > kMaxSensors) {
                auto oldest = std::min_element(
                    sensors_.begin(), sensors_.end(),
                    [](const Sensor& a, const Sensor& b) {
                        return a.last_seen < b.last_seen;
                    });
                if (oldest != sensors_.end()) sensors_.erase(oldest);
                else break;
            }
        }
        std::sort(sensors_.begin(), sensors_.end(),
                  [](const Sensor& a, const Sensor& b) {
                      return a.last_seen > b.last_seen;
                  });
    }
    total_.fetch_add(1);
}

std::vector<Sensor> Rtl433Controller::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sensors_;
}

std::vector<Sensor> Rtl433Controller::snapshotFiltered(Filter filter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (filter == Filter::All) return sensors_;
    std::vector<Sensor> out;
    out.reserve(sensors_.size());
    for (const auto& s : sensors_) {
        if (filter == Filter::Tpms && s.kind == Kind::Tpms) out.push_back(s);
        else if (filter == Filter::Weather && s.kind == Kind::Weather) {
            out.push_back(s);
        } else if (filter == Filter::Other && s.kind == Kind::Other) {
            out.push_back(s);
        }
    }
    return out;
}

}  // namespace rf433
