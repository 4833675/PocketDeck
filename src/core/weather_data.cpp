#include "core/weather_data.h"

#include <cstdio>

namespace pd {
namespace {

const char* weatherFailureStageLabel(WeatherFailureStage stage) {
    switch (stage) {
        case WeatherFailureStage::None: return "NONE";
        case WeatherFailureStage::Wifi: return "WIFI";
        case WeatherFailureStage::Task: return "TASK";
        case WeatherFailureStage::Dns: return "DNS";
        case WeatherFailureStage::Endpoint: return "ENDPOINT";
        case WeatherFailureStage::ConnectTls: return "CONNECT_TLS";
        case WeatherFailureStage::HttpTransport: return "HTTP_TRANSPORT";
        case WeatherFailureStage::HttpResponse: return "HTTP_RESPONSE";
        case WeatherFailureStage::Parse: return "PARSE";
        case WeatherFailureStage::Fields: return "FIELDS";
    }
    return "UNKNOWN";
}

const char* weatherHttpStatusLabel(int16_t status) {
    switch (status) {
        case 0: return "NONE";
        case -1: return "CONNECT_FAILED";
        case -2: return "SEND_HEADER_FAILED";
        case -3: return "SEND_PAYLOAD_FAILED";
        case -4: return "NOT_CONNECTED";
        case -5: return "CONNECTION_LOST";
        case -6: return "NO_STREAM";
        case -7: return "NO_HTTP_SERVER";
        case -8: return "LOW_MEMORY";
        case -9: return "ENCODING";
        case -10: return "STREAM_WRITE";
        case -11: return "READ_TIMEOUT";
        case 200: return "OK";
        default: return status > 0 ? "HTTP_RESPONSE" : "TRANSPORT_ERROR";
    }
}

}  // namespace

const char* weatherStateLabel(WeatherState state) {
    switch (state) {
        case WeatherState::Idle: return "WAITING";
        case WeatherState::Fetching: return "FETCHING";
        case WeatherState::Ready: return "READY";
        case WeatherState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* weatherCodeLabel(uint8_t code) {
    if (code == 0) return "CLEAR";
    if (code == 1) return "MOSTLY CLEAR";
    if (code == 2) return "PARTLY CLOUDY";
    if (code == 3) return "OVERCAST";
    if (code == 45 || code == 48) return "FOG";
    if (code >= 51 && code <= 57) return "DRIZZLE";
    if (code >= 61 && code <= 67) return "RAIN";
    if (code >= 71 && code <= 77) return "SNOW";
    if (code >= 80 && code <= 82) return "SHOWERS";
    if (code == 85 || code == 86) return "SNOW SHOWERS";
    if (code >= 95) return "THUNDERSTORM";
    return "UNKNOWN";
}

WeatherDisplayState classifyWeatherDisplay(const WeatherSnapshot& weather,
                                           bool wifiEnabled, bool wifiConnected,
                                           bool gpsFresh) {
    if (weather.valid) {
        if (weather.state == WeatherState::Fetching) return WeatherDisplayState::Updating;
        if (weather.state == WeatherState::Error) return WeatherDisplayState::CachedError;
        if (!wifiConnected) return WeatherDisplayState::CachedOffline;
        if (!gpsFresh) return WeatherDisplayState::CachedNoGps;
        return WeatherDisplayState::Live;
    }

    if (!wifiEnabled) return WeatherDisplayState::WifiOff;
    if (!wifiConnected) return WeatherDisplayState::NoNetwork;
    if (!gpsFresh) return WeatherDisplayState::WaitingGps;
    if (weather.state == WeatherState::Fetching) return WeatherDisplayState::Fetching;
    if (weather.state == WeatherState::Error) return WeatherDisplayState::Error;
    return WeatherDisplayState::ReadyToFetch;
}

bool weatherDisplayShowsData(WeatherDisplayState state) {
    return state == WeatherDisplayState::Live || state == WeatherDisplayState::Updating ||
           state == WeatherDisplayState::CachedNoGps ||
           state == WeatherDisplayState::CachedOffline ||
           state == WeatherDisplayState::CachedError;
}

bool formatWeatherFailureDiagnostics(const WeatherFailureDiagnostics& diagnostics,
                                     char* output, std::size_t capacity) {
    if (output == nullptr || capacity == 0) return false;
    const int written = std::snprintf(
        output, capacity,
        "stage=%s dns=%s/%lums http=%d(%s) tls=%ld request=%lums "
        "heap=%lu/%lu->%lu/%lu dma=%lu/%lu->%lu/%lu",
        weatherFailureStageLabel(diagnostics.stage),
        diagnostics.dnsResolved ? "ok" : "fail",
        static_cast<unsigned long>(diagnostics.dnsElapsedMs),
        static_cast<int>(diagnostics.httpStatus),
        weatherHttpStatusLabel(diagnostics.httpStatus),
        static_cast<long>(diagnostics.tlsError),
        static_cast<unsigned long>(diagnostics.requestElapsedMs),
        static_cast<unsigned long>(diagnostics.freeHeapBefore),
        static_cast<unsigned long>(diagnostics.largestHeapBefore),
        static_cast<unsigned long>(diagnostics.freeHeapAfter),
        static_cast<unsigned long>(diagnostics.largestHeapAfter),
        static_cast<unsigned long>(diagnostics.dmaFreeBefore),
        static_cast<unsigned long>(diagnostics.dmaLargestBefore),
        static_cast<unsigned long>(diagnostics.dmaFreeAfter),
        static_cast<unsigned long>(diagnostics.dmaLargestAfter));
    return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

bool formatWeatherRouteDiagnostics(const WeatherFailureDiagnostics& diagnostics,
                                   char* output, std::size_t capacity) {
    if (output == nullptr || capacity == 0) return false;
    const int written = std::snprintf(
        output, capacity,
        "dns=%s/%lums ip=%u.%u.%u.%u",
        diagnostics.dnsResolved ? "ok" : "fail",
        static_cast<unsigned long>(diagnostics.dnsElapsedMs),
        static_cast<unsigned>(diagnostics.resolvedAddress[0]),
        static_cast<unsigned>(diagnostics.resolvedAddress[1]),
        static_cast<unsigned>(diagnostics.resolvedAddress[2]),
        static_cast<unsigned>(diagnostics.resolvedAddress[3]));
    return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

}  // namespace pd
