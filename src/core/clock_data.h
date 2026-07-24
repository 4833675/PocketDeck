#pragma once

#include <cstdint>

namespace pd {

struct ClockDisplay {
    bool valid = false;
    uint8_t hour = 0;
    uint8_t minute = 0;
};

ClockDisplay clockFromUtcEpoch(int64_t utcEpoch, int32_t utcOffsetSeconds);

}  // namespace pd
