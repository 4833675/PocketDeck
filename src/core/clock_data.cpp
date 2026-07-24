#include "core/clock_data.h"

namespace pd {
namespace {

constexpr int64_t kPlausibleEpoch = 1704067200;  // 2024-01-01 UTC
constexpr int64_t kSecondsPerDay = 24 * 60 * 60;

}  // namespace

ClockDisplay clockFromUtcEpoch(int64_t utcEpoch, int32_t utcOffsetSeconds) {
    ClockDisplay display;
    if (utcEpoch < kPlausibleEpoch) return display;

    int64_t secondsToday = (utcEpoch + utcOffsetSeconds) % kSecondsPerDay;
    if (secondsToday < 0) secondsToday += kSecondsPerDay;
    display.valid = true;
    display.hour = static_cast<uint8_t>(secondsToday / 3600);
    display.minute = static_cast<uint8_t>((secondsToday % 3600) / 60);
    return display;
}

}  // namespace pd
