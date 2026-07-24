#include "services/weather_service.h"

#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace pd {
namespace {

constexpr uint32_t kWeatherTaskStack = 12288;

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const char* source) {
    destination.fill('\0');
    if (source != nullptr) std::strncpy(destination.data(), source, Size - 1);
}

template <std::size_t Size>
void copyClock(std::array<char, Size>& destination, const char* isoTime) {
    destination.fill('\0');
    if (isoTime == nullptr) return;
    const char* time = std::strchr(isoTime, 'T');
    if (time == nullptr || std::strlen(time + 1) < 5) return;
    std::snprintf(destination.data(), destination.size(), "%.5s", time + 1);
}

}  // namespace

bool WeatherService::begin() {
    WeatherSnapshot initial;
    initial.state = WeatherState::Idle;
    publish(initial);
    return true;
}

bool WeatherService::requestRefresh(double latitude, double longitude) {
    if (WiFi.status() != WL_CONNECTED) {
        publishError("Wi-Fi is not connected");
        return false;
    }
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) return false;

    pendingLatitude_ = latitude;
    pendingLongitude_ = longitude;
    WeatherSnapshot fetching = snapshot();
    fetching.state = WeatherState::Fetching;
    fetching.error.fill('\0');
    publish(fetching);

    if (xTaskCreate(fetchTask, "weather-fetch", kWeatherTaskStack, this, 1, nullptr) != pdPASS) {
        busy_.store(false);
        publishError("Unable to start weather task");
        return false;
    }
    return true;
}

WeatherSnapshot WeatherService::snapshot() const {
    portENTER_CRITICAL(&mux_);
    const WeatherSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

void WeatherService::fetchTask(void* context) {
    auto* service = static_cast<WeatherService*>(context);
    service->performFetch();
    service->busy_.store(false);
    vTaskDelete(nullptr);
}

void WeatherService::performFetch() {
    if (WiFi.status() != WL_CONNECTED) {
        publishError("Wi-Fi disconnected");
        return;
    }

    char url[512];
    std::snprintf(
        url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
        pendingLatitude_, pendingLongitude_);

    // The endpoint carries public, non-sensitive forecast data. Avoiding a
    // pinned certificate keeps the field device working across CA rotations.
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(8);
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(10000);
    http.useHTTP10(true);
    if (!http.begin(client, url)) {
        publishError("Unable to open weather endpoint");
        return;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        publishError("Weather HTTP request failed", static_cast<int16_t>(status));
        return;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, http.getStream());
    if (error) {
        http.end();
        publishError("Weather response was invalid", static_cast<int16_t>(status));
        return;
    }

    JsonVariantConst current = document["current"];
    JsonVariantConst daily = document["daily"];
    if (current.isNull() || daily.isNull()) {
        http.end();
        publishError("Weather fields were missing", static_cast<int16_t>(status));
        return;
    }

    WeatherSnapshot result;
    result.state = WeatherState::Ready;
    result.valid = true;
    result.latitude = document["latitude"] | pendingLatitude_;
    result.longitude = document["longitude"] | pendingLongitude_;
    result.temperatureC = current["temperature_2m"] | 0.0f;
    result.apparentC = current["apparent_temperature"] | 0.0f;
    result.humidityPercent = current["relative_humidity_2m"] | 0;
    result.weatherCode = current["weather_code"] | 0;
    result.windKph = current["wind_speed_10m"] | 0.0f;
    result.highC = daily["temperature_2m_max"][0] | result.temperatureC;
    result.lowC = daily["temperature_2m_min"][0] | result.temperatureC;
    result.httpStatus = static_cast<int16_t>(status);
    result.updatedAtMs = millis();
    copyText(result.timezone, document["timezone"] | "local");
    copyText(result.observationTime, current["time"] | "");
    copyClock(result.sunrise, daily["sunrise"][0] | "");
    copyClock(result.sunset, daily["sunset"][0] | "");
    http.end();
    publish(result);
}

void WeatherService::publish(const WeatherSnapshot& snapshot) {
    portENTER_CRITICAL(&mux_);
    snapshot_ = snapshot;
    portEXIT_CRITICAL(&mux_);
}

void WeatherService::publishError(const char* message, int16_t httpStatus) {
    WeatherSnapshot result = snapshot();
    result.state = WeatherState::Error;
    result.httpStatus = httpStatus;
    copyText(result.error, message);
    publish(result);
}

}  // namespace pd
