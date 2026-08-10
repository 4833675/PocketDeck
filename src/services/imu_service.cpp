#include "services/imu_service.h"

#include <M5Unified.h>

namespace pd {
namespace {

constexpr uint32_t kSampleIntervalMs = 20u;

}  // namespace

bool ImuService::begin() {
    if (begun_) return snapshot_.available;

    snapshot_.available = M5.Imu.isEnabled();
    snapshot_.active = false;
    begun_ = true;
    return snapshot_.available;
}

void ImuService::setActive(bool active) {
    const bool nextActive = active && snapshot_.available;
    if (snapshot_.active == nextActive) return;

    snapshot_.active = nextActive;
}

void ImuService::update(uint32_t nowMs) {
    if (!snapshot_.active || !snapshot_.available) return;
    if (hasReadAttempt_ &&
        static_cast<uint32_t>(nowMs - lastReadMs_) < kSampleIntervalMs) {
        return;
    }

    hasReadAttempt_ = true;
    lastReadMs_ = nowMs;

    MotionSample sample;
    const bool accelRead = M5.Imu.getAccel(&sample.ax, &sample.ay, &sample.az);
    const bool gyroRead = M5.Imu.getGyro(&sample.gx, &sample.gy, &sample.gz);
    snapshot_.fresh = accelRead && gyroRead;
    if (!snapshot_.fresh) return;

    classifier_.update(sample, nowMs);
    snapshot_.hasSample = true;
    snapshot_.ax = sample.ax;
    snapshot_.ay = sample.ay;
    snapshot_.az = sample.az;
    snapshot_.gx = sample.gx;
    snapshot_.gy = sample.gy;
    snapshot_.gz = sample.gz;
    snapshot_.lastSampleMs = nowMs;
    refreshSnapshot();
}

void ImuService::zeroLevel() {
    if (!snapshot_.hasSample) return;

    zeroRollDegrees_ = classifier_.level().rollDegrees;
    zeroPitchDegrees_ = classifier_.level().pitchDegrees;
    refreshSnapshot();
}

void ImuService::resetPeak() {
    classifier_.resetPeak();
    refreshSnapshot();
}

void ImuService::refreshSnapshot() {
    snapshot_.rollDegrees = classifier_.level().rollDegrees - zeroRollDegrees_;
    snapshot_.pitchDegrees = classifier_.level().pitchDegrees - zeroPitchDegrees_;
    snapshot_.accelerationMagnitude = classifier_.accelerationMagnitude();
    snapshot_.accelerationDeviation = classifier_.accelerationDeviation();
    snapshot_.gyroMagnitude = classifier_.gyroMagnitude();
    snapshot_.activity = classifier_.activity();
    snapshot_.peakAccelerationDeviation = classifier_.peakAccelerationDeviation();
}

}  // namespace pd
