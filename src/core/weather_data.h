#pragma once

#include <array>
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
};

const char* weatherStateLabel(WeatherState state);
const char* weatherCodeLabel(uint8_t code);
WeatherDisplayState classifyWeatherDisplay(const WeatherSnapshot& weather,
                                           bool wifiEnabled, bool wifiConnected,
                                           bool gpsFresh);
bool weatherDisplayShowsData(WeatherDisplayState state);

}  // namespace pd
