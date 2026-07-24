#include "core/weather_data.h"

namespace pd {

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

}  // namespace pd
