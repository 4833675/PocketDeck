#include "services/gps_service.h"

#include <Arduino.h>

namespace pd {
namespace {

constexpr uint32_t kGpsBaud = 115200;
constexpr int kGpsRxPin = 15;
constexpr int kGpsTxPin = 13;
constexpr uint16_t kMaxBytesPerUpdate = 2048;

}  // namespace

bool GpsService::begin() {
    Serial1.begin(kGpsBaud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
    snapshot_.started = static_cast<bool>(Serial1);
    active_ = snapshot_.started;
    refreshSnapshot(millis());
    return snapshot_.started;
}

void GpsService::setActive(bool active) {
    const bool nextActive = active && snapshot_.started;
    if (active_ == nextActive) return;

    if (nextActive) {
        // Keep the UART driver configured while suspended. Repeated end/begin
        // cycles would churn scarce heap, and they do not power down the Cap's
        // GPS receiver. Drop only the stale RX backlog before accepting fresh
        // NMEA; TinyGPS++ will resynchronize at the next sentence boundary.
        Serial1.flush(false);
        refreshSnapshot(millis());
    }
    active_ = nextActive;
}

void GpsService::update() {
    if (!active_ || !snapshot_.started) return;
    const uint32_t nowMs = millis();
    uint16_t processed = 0;
    while (Serial1.available() > 0 && processed < kMaxBytesPerUpdate) {
        const int value = Serial1.read();
        if (value < 0) break;
        parser_.encode(static_cast<char>(value));
        lastByteMs_ = nowMs;
        ++processed;
    }
    refreshSnapshot(nowMs);
}

void GpsService::refreshSnapshot(uint32_t nowMs) {
    snapshot_.charsProcessed = parser_.charsProcessed();
    snapshot_.sentencesWithFix = parser_.sentencesWithFix();
    snapshot_.sentencesPassed = parser_.passedChecksum();
    snapshot_.checksumFailed = parser_.failedChecksum();
    snapshot_.dataAgeMs = snapshot_.charsProcessed == 0 ? 0xFFFFFFFFu : nowMs - lastByteMs_;

    snapshot_.locationValid = parser_.location.isValid();
    snapshot_.locationAgeMs = parser_.location.age();
    if (snapshot_.locationValid) {
        snapshot_.latitude = parser_.location.lat();
        snapshot_.longitude = parser_.location.lng();
        snapshot_.fixQuality = static_cast<char>(parser_.location.FixQuality());
        snapshot_.fixMode = static_cast<char>(parser_.location.FixMode());
    }

    snapshot_.altitudeValid = parser_.altitude.isValid();
    if (snapshot_.altitudeValid) snapshot_.altitudeMeters = parser_.altitude.meters();

    snapshot_.satellitesValid = parser_.satellites.isValid();
    if (snapshot_.satellitesValid) snapshot_.satellites = parser_.satellites.value();

    snapshot_.hdopValid = parser_.hdop.isValid();
    if (snapshot_.hdopValid) snapshot_.hdop = parser_.hdop.hdop();

    snapshot_.speedValid = parser_.speed.isValid();
    if (snapshot_.speedValid) snapshot_.speedKph = parser_.speed.kmph();

    snapshot_.courseValid = parser_.course.isValid();
    if (snapshot_.courseValid) snapshot_.courseDegrees = parser_.course.deg();

    snapshot_.dateValid = parser_.date.isValid();
    if (snapshot_.dateValid) {
        snapshot_.year = parser_.date.year();
        snapshot_.month = parser_.date.month();
        snapshot_.day = parser_.date.day();
    }

    snapshot_.timeValid = parser_.time.isValid();
    if (snapshot_.timeValid) {
        snapshot_.hour = parser_.time.hour();
        snapshot_.minute = parser_.time.minute();
        snapshot_.second = parser_.time.second();
        snapshot_.centisecond = parser_.time.centisecond();
    }
}

}  // namespace pd
