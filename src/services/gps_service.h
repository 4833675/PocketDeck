#pragma once

#include <cstdint>

#include <TinyGPS++.h>

#include "core/gps_data.h"

namespace pd {

class GpsService {
public:
    bool begin();
    void update();
    const GpsSnapshot& snapshot() const { return snapshot_; }

private:
    void refreshSnapshot(uint32_t nowMs);

    TinyGPSPlus parser_;
    GpsSnapshot snapshot_{};
    uint32_t lastByteMs_ = 0;
};

}  // namespace pd
