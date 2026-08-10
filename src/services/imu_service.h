#pragma once

#include <cstdint>

#include "core/motion_data.h"

namespace pd {

struct ImuSnapshot {
    bool available = false;
    bool active = false;
    bool fresh = false;
    bool hasSample = false;
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    float rollDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float accelerationMagnitude = 0.0f;
    float accelerationDeviation = 0.0f;
    float gyroMagnitude = 0.0f;
    MotionActivity activity = MotionActivity::Moving;
    float peakAccelerationDeviation = 0.0f;
    uint32_t lastSampleMs = 0;
};

class ImuService {
public:
    bool begin();
    void setActive(bool active);
    bool active() const { return snapshot_.active; }
    void update(uint32_t nowMs);
    void zeroLevel();
    void resetPeak();
    const ImuSnapshot& snapshot() const { return snapshot_; }

private:
    void refreshSnapshot();

    MotionClassifier classifier_;
    ImuSnapshot snapshot_{};
    float zeroRollDegrees_ = 0.0f;
    float zeroPitchDegrees_ = 0.0f;
    uint32_t lastReadMs_ = 0;
    bool begun_ = false;
    bool hasReadAttempt_ = false;
};

}  // namespace pd
