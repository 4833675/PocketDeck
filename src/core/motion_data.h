#pragma once

#include <cstdint>

namespace pd {

struct MotionSample {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
};

enum class MotionActivity : uint8_t {
    Still,
    Moving,
    Shake,
};

struct MotionLevel {
    float rollDegrees = 0.0f;
    float pitchDegrees = 0.0f;
};

bool motionSampleIsCurrent(bool hasSample, uint32_t nowMs, uint32_t lastSampleMs);

class MotionClassifier {
public:
    void update(const MotionSample& sample, uint32_t nowMs);

    const MotionLevel& level() const { return level_; }
    MotionActivity activity() const { return activity_; }
    float accelerationMagnitude() const { return accelerationMagnitude_; }
    float accelerationDeviation() const { return accelerationDeviation_; }
    float gyroMagnitude() const { return gyroMagnitude_; }
    float peakAccelerationDeviation() const { return peakAccelerationDeviation_; }
    void resetPeak();

private:
    MotionActivity classify() const;
    void applyHysteresis(MotionActivity candidate);

    MotionLevel level_{};
    MotionActivity activity_ = MotionActivity::Moving;
    MotionActivity pendingActivity_ = MotionActivity::Moving;
    uint8_t pendingSamples_ = 0;
    float accelerationMagnitude_ = 0.0f;
    float accelerationDeviation_ = 0.0f;
    float gyroMagnitude_ = 0.0f;
    float peakAccelerationDeviation_ = 0.0f;
    uint32_t shakeStartedAtMs_ = 0;
    bool hasSample_ = false;
    bool shakeLatched_ = false;
};

}  // namespace pd
