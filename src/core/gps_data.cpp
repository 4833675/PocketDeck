#include "core/gps_data.h"

#include <cmath>

namespace pd {

GpsState classifyGpsState(const GpsSnapshot& snapshot) {
    if (snapshot.charsProcessed == 0) return GpsState::NoData;
    if (snapshot.dataAgeMs > 3000u) return GpsState::NoStream;
    if (!snapshot.locationValid) return GpsState::Searching;
    if (snapshot.locationAgeMs > 5000u) return GpsState::Stale;
    return GpsState::Fix;
}

const char* gpsStateLabel(GpsState state) {
    switch (state) {
        case GpsState::NoData: return "NO DATA";
        case GpsState::NoStream: return "NO STREAM";
        case GpsState::Searching: return "SEARCHING";
        case GpsState::Stale: return "STALE FIX";
        case GpsState::Fix: return "FIX";
    }
    return "UNKNOWN";
}

const char* gpsCompassPoint(double courseDegrees) {
    if (!std::isfinite(courseDegrees)) return "--";
    static constexpr const char* kPoints[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    double normalized = std::fmod(courseDegrees, 360.0);
    if (normalized < 0.0) normalized += 360.0;
    const int index = static_cast<int>((normalized + 22.5) / 45.0) % 8;
    return kPoints[index];
}

const char* gpsFixQualityLabel(char quality) {
    switch (quality) {
        case '0': return "NONE";
        case '1': return "GPS";
        case '2': return "DGPS";
        case '3': return "PPS";
        case '4': return "RTK";
        case '5': return "FLOAT";
        case '6': return "EST";
        case '7': return "MANUAL";
        case '8': return "SIM";
        default: return "--";
    }
}

const char* gpsFixModeLabel(char mode) {
    switch (mode) {
        case 'N': return "NONE";
        case 'A': return "AUTON";
        case 'D': return "DIFF";
        case 'E': return "EST";
        default: return "--";
    }
}

}  // namespace pd
