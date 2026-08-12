#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>

#include "core/weather_data.h"

namespace pd {

class WeatherService {
public:
    bool begin();
    bool requestRefresh(double latitude, double longitude);
    WeatherSnapshot snapshot() const;

private:
    static void fetchTask(void* context);
    void performFetch();
    void publish(const WeatherSnapshot& snapshot);
    void publishError(const char* message,
                      const WeatherFailureDiagnostics& diagnostics);

    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    WeatherSnapshot snapshot_{};
    std::atomic<bool> busy_{false};
    double pendingLatitude_ = 0.0;
    double pendingLongitude_ = 0.0;
};

}  // namespace pd
