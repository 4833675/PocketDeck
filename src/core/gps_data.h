#pragma once

#include <cstdint>

namespace pd {

struct GpsSnapshot {
    bool started = false;
    bool locationValid = false;
    bool altitudeValid = false;
    bool satellitesValid = false;
    bool hdopValid = false;
    bool speedValid = false;
    bool courseValid = false;
    bool dateValid = false;
    bool timeValid = false;

    double latitude = 0.0;
    double longitude = 0.0;
    double altitudeMeters = 0.0;
    double hdop = 0.0;
    double speedKph = 0.0;
    double courseDegrees = 0.0;
    uint32_t satellites = 0;
    char fixQuality = '0';
    char fixMode = 'N';

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    uint8_t centisecond = 0;

    uint32_t charsProcessed = 0;
    uint32_t sentencesWithFix = 0;
    uint32_t sentencesPassed = 0;
    uint32_t checksumFailed = 0;
    uint32_t dataAgeMs = 0xFFFFFFFFu;
    uint32_t locationAgeMs = 0xFFFFFFFFu;
};

enum class GpsState : uint8_t {
    NoData,
    NoStream,
    Searching,
    Stale,
    Fix,
};

GpsState classifyGpsState(const GpsSnapshot& snapshot);
const char* gpsStateLabel(GpsState state);
const char* gpsCompassPoint(double courseDegrees);
const char* gpsFixQualityLabel(char quality);
const char* gpsFixModeLabel(char mode);

}  // namespace pd
