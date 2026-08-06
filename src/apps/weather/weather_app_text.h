#pragma once

#include <cstdint>

#include "core/localization.h"
#include "core/weather_data.h"
#include "core/wifi_data.h"

namespace pd {

const char* localizedWeatherCodeLabel(uint8_t code, UiLanguage language);
const char* localizedWeatherDisplayLabel(WeatherDisplayState state,
                                         UiLanguage language);
const char* localizedWeatherWifiStateLabel(WifiState state,
                                           UiLanguage language);
const char* localizedWeatherErrorLabel(const char* error,
                                       UiLanguage language);

}  // namespace pd
