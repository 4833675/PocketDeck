#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

enum class WeatherState : uint8_t {
    Idle,
    Fetching,
    Ready,
    Error,
};

enum class WeatherDisplayState : uint8_t {
    Live,
    Updating,
    CachedNoGps,
    CachedOffline,
    CachedError,
    WifiOff,
    NoNetwork,
    WaitingGps,
    Fetching,
    Error,
    ReadyToFetch,
};

enum class WeatherFailureStage : uint8_t {
    None,
    Wifi,
    Task,
    Dns,
    Endpoint,
    ConnectTls,
    HttpTransport,
    HttpResponse,
    Parse,
    Fields,
};

struct WeatherFailureDiagnostics {
    WeatherFailureStage stage = WeatherFailureStage::None;
    bool dnsResolved = false;
    std::array<uint8_t, 4> resolvedAddress{};
    int16_t httpStatus = 0;
    int32_t tlsError = 0;
    uint32_t dnsElapsedMs = 0;
    uint32_t requestElapsedMs = 0;
    uint32_t freeHeapBefore = 0;
    uint32_t largestHeapBefore = 0;
    uint32_t freeHeapAfter = 0;
    uint32_t largestHeapAfter = 0;
    uint32_t dmaFreeBefore = 0;
    uint32_t dmaLargestBefore = 0;
    uint32_t dmaFreeAfter = 0;
    uint32_t dmaLargestAfter = 0;
};

struct WeatherSnapshot {
    WeatherState state = WeatherState::Idle;
    bool valid = false;
    double latitude = 0.0;
    double longitude = 0.0;
    float temperatureC = 0.0f;
    float apparentC = 0.0f;
    float highC = 0.0f;
    float lowC = 0.0f;
    float windKph = 0.0f;
    uint8_t humidityPercent = 0;
    uint8_t weatherCode = 0;
    int16_t httpStatus = 0;
    uint32_t updatedAtMs = 0;
    std::array<char, 32> timezone{};
    std::array<char, 20> observationTime{};
    std::array<char, 6> sunrise{};
    std::array<char, 6> sunset{};
    std::array<char, 40> error{};
    WeatherFailureDiagnostics failureDiagnostics{};
};

const char* weatherStateLabel(WeatherState state);
const char* weatherCodeLabel(uint8_t code);
WeatherDisplayState classifyWeatherDisplay(const WeatherSnapshot& weather,
                                           bool wifiEnabled, bool wifiConnected,
                                           bool gpsFresh);
bool weatherDisplayShowsData(WeatherDisplayState state);
bool formatWeatherFailureDiagnostics(const WeatherFailureDiagnostics& diagnostics,
                                     char* output, std::size_t capacity);
bool formatWeatherRouteDiagnostics(const WeatherFailureDiagnostics& diagnostics,
                                   char* output, std::size_t capacity);

}  // namespace pd
