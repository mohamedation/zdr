#ifndef SAMPLE_RATE_H
#define SAMPLE_RATE_H

#include <cstdint>

// Capture rates that are multiples of 240 kHz decimate to exactly 48 kHz
// audio (240 kHz / 5). Other rates (e.g. classic 1.024 Msps) produce 51.2 kHz
// worth of samples against a 48 kHz ALSA device and cause periodic dropouts.

// valid range is ~900 kHz–3.2 MHz (plus a separate 225–300 kHz low band).

namespace SampleRate {

constexpr uint32_t kAudioOutHz = 48000;
constexpr uint32_t kAudioRateStep = 240000;       // mid rate before /5 → 48 kHz
constexpr uint32_t kDefaultHz = 1200000;          // exact 48 kHz, near old 1.024M
constexpr uint32_t kMinHighBandHz = 4 * kAudioRateStep;   // 960 kHz
constexpr uint32_t kMaxHighBandHz = 13 * kAudioRateStep;  // 3.12 MHz

inline bool isExactAudioRate(uint32_t rate_hz) {
    return rate_hz >= kAudioRateStep && (rate_hz % kAudioRateStep) == 0;
}

// Snap to the nearest multiple of 240 kHz inside the RTL high band.
// Used for CLI -s and for migrating saved 1.024 Msps configs.
inline uint32_t snap(uint32_t rate_hz) {
    if (rate_hz == 0) {
        return kDefaultHz;
    }

    uint64_t rounded =
        ((static_cast<uint64_t>(rate_hz) + kAudioRateStep / 2) / kAudioRateStep) *
        kAudioRateStep;
    if (rounded < kMinHighBandHz) {
        rounded = kMinHighBandHz;
    }
    if (rounded > kMaxHighBandHz) {
        rounded = kMaxHighBandHz;
    }
    return static_cast<uint32_t>(rounded);
}

}  // namespace SampleRate

#endif  // SAMPLE_RATE_H
