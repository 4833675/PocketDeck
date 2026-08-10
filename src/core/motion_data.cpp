#include "core/motion_data.h"

#include <cmath>

namespace pd {
namespace {

constexpr float kDegreesPerRadian = 57.2957795131f;
constexpr float kLevelFilterAlpha = 0.2f;
constexpr float kStillAccelerationDeviation = 0.08f;
constexpr float kStillGyroMagnitude = 10.0f;
constexpr float kShakeAccelerationDeviation = 0.45f;
constexpr float kShakeGyroMagnitude = 180.0f;
constexpr uint8_t kHysteresisSamples = 5;
constexpr uint32_t kShakeLatchMs = 500u;
constexpr uint32_t kSampleCurrentMs = 500u;

float magnitude(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

}  // namespace

bool motionSampleIsCurrent(bool hasSample, uint32_t nowMs, uint32_t lastSampleMs) {
    return hasSample && static_cast<uint32_t>(nowMs - lastSampleMs) <= kSampleCurrentMs;
}

void MotionClassifier::update(const MotionSample& sample, uint32_t nowMs) {
    const MotionLevel rawLevel{
        std::atan2(sample.ay, sample.az) * kDegreesPerRadian,
        std::atan2(-sample.ax, std::sqrt(sample.ay * sample.ay + sample.az * sample.az)) *
            kDegreesPerRadian,
    };
    if (!hasSample_) {
        level_ = rawLevel;
    } else {
        level_.rollDegrees += kLevelFilterAlpha * (rawLevel.rollDegrees - level_.rollDegrees);
        level_.pitchDegrees +=
            kLevelFilterAlpha * (rawLevel.pitchDegrees - level_.pitchDegrees);
    }

    accelerationMagnitude_ = magnitude(sample.ax, sample.ay, sample.az);
    accelerationDeviation_ = std::fabs(accelerationMagnitude_ - 1.0f);
    gyroMagnitude_ = magnitude(sample.gx, sample.gy, sample.gz);
    if (!hasSample_ || accelerationDeviation_ > peakAccelerationDeviation_) {
        peakAccelerationDeviation_ = accelerationDeviation_;
    }
    hasSample_ = true;

    if (shakeLatched_ && static_cast<uint32_t>(nowMs - shakeStartedAtMs_) >= kShakeLatchMs) {
        shakeLatched_ = false;
        pendingSamples_ = 0;
    }

    const MotionActivity candidate = classify();
    if (candidate == MotionActivity::Shake) {
        if (!shakeLatched_) {
            shakeLatched_ = true;
            shakeStartedAtMs_ = nowMs;
        }
        activity_ = MotionActivity::Shake;
        pendingSamples_ = 0;
        return;
    }

    if (shakeLatched_) return;
    applyHysteresis(candidate);
}

void MotionClassifier::resetPeak() {
    peakAccelerationDeviation_ = hasSample_ ? accelerationDeviation_ : 0.0f;
}

MotionActivity MotionClassifier::classify() const {
    if (accelerationDeviation_ < kStillAccelerationDeviation &&
        gyroMagnitude_ < kStillGyroMagnitude) {
        return MotionActivity::Still;
    }
    if (accelerationDeviation_ >= kShakeAccelerationDeviation ||
        gyroMagnitude_ >= kShakeGyroMagnitude) {
        return MotionActivity::Shake;
    }
    return MotionActivity::Moving;
}

void MotionClassifier::applyHysteresis(MotionActivity candidate) {
    if (candidate == activity_) {
        pendingSamples_ = 0;
        return;
    }

    if (candidate == pendingActivity_) {
        ++pendingSamples_;
    } else {
        pendingActivity_ = candidate;
        pendingSamples_ = 1;
    }

    if (pendingSamples_ >= kHysteresisSamples) {
        activity_ = candidate;
        pendingSamples_ = 0;
    }
}

}  // namespace pd
