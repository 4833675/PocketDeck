#include "services/weather_service.h"

#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

namespace pd {
namespace {

constexpr uint32_t kWeatherTaskStack = 12288;
constexpr const char* kWeatherHost = "api.open-meteo.com";

void captureHeapBefore(WeatherFailureDiagnostics& diagnostics) {
    diagnostics.freeHeapBefore = ESP.getFreeHeap();
    diagnostics.largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    constexpr uint32_t kDmaCaps = MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL;
    diagnostics.dmaFreeBefore = heap_caps_get_free_size(kDmaCaps);
    diagnostics.dmaLargestBefore = heap_caps_get_largest_free_block(kDmaCaps);
}

void captureHeapAfter(WeatherFailureDiagnostics& diagnostics) {
    diagnostics.freeHeapAfter = ESP.getFreeHeap();
    diagnostics.largestHeapAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    constexpr uint32_t kDmaCaps = MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL;
    diagnostics.dmaFreeAfter = heap_caps_get_free_size(kDmaCaps);
    diagnostics.dmaLargestAfter = heap_caps_get_largest_free_block(kDmaCaps);
}

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
        WeatherFailureDiagnostics diagnostics;
        diagnostics.stage = WeatherFailureStage::Wifi;
        captureHeapBefore(diagnostics);
        captureHeapAfter(diagnostics);
        publishError("Wi-Fi is not connected", diagnostics);
        return false;
    }
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) return false;

    pendingLatitude_ = latitude;
    pendingLongitude_ = longitude;
    WeatherSnapshot fetching = snapshot();
    fetching.state = WeatherState::Fetching;
    fetching.httpStatus = 0;
    fetching.error.fill('\0');
    fetching.failureDiagnostics = {};
    publish(fetching);

    if (xTaskCreate(fetchTask, "weather-fetch", kWeatherTaskStack, this, 1, nullptr) != pdPASS) {
        busy_.store(false);
        WeatherFailureDiagnostics diagnostics;
        diagnostics.stage = WeatherFailureStage::Task;
        captureHeapBefore(diagnostics);
        captureHeapAfter(diagnostics);
        publishError("Unable to start weather task", diagnostics);
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
    WeatherFailureDiagnostics diagnostics;
    captureHeapBefore(diagnostics);
    if (WiFi.status() != WL_CONNECTED) {
        diagnostics.stage = WeatherFailureStage::Wifi;
        captureHeapAfter(diagnostics);
        publishError("Wi-Fi disconnected", diagnostics);
        return;
    }

    IPAddress resolvedAddress;
    const uint32_t dnsStartedMs = millis();
    diagnostics.dnsResolved = WiFi.hostByName(kWeatherHost, resolvedAddress) == 1;
    diagnostics.dnsElapsedMs = millis() - dnsStartedMs;
    if (!diagnostics.dnsResolved) {
        diagnostics.stage = WeatherFailureStage::Dns;
        captureHeapAfter(diagnostics);
        publishError("Weather DNS lookup failed", diagnostics);
        return;
    }
    for (std::size_t index = 0; index < diagnostics.resolvedAddress.size(); ++index) {
        diagnostics.resolvedAddress[index] = resolvedAddress[index];
    }

    char url[512];
    std::snprintf(
        url, sizeof(url),
        "https://%s/v1/forecast?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
        kWeatherHost, pendingLatitude_, pendingLongitude_);

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
        diagnostics.stage = WeatherFailureStage::Endpoint;
        captureHeapAfter(diagnostics);
        publishError("Unable to open weather endpoint", diagnostics);
        return;
    }

    const uint32_t requestStartedMs = millis();
    const int status = http.GET();
    diagnostics.requestElapsedMs = millis() - requestStartedMs;
    diagnostics.httpStatus = static_cast<int16_t>(status);
    char tlsErrorText[64]{};
    diagnostics.tlsError = client.lastError(tlsErrorText, sizeof(tlsErrorText));
    captureHeapAfter(diagnostics);
    if (status != HTTP_CODE_OK) {
        diagnostics.stage = status == HTTPC_ERROR_CONNECTION_REFUSED
                                ? WeatherFailureStage::ConnectTls
                                : (status < 0 ? WeatherFailureStage::HttpTransport
                                              : WeatherFailureStage::HttpResponse);
        http.end();
        publishError("Weather HTTP request failed", diagnostics);
        return;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, http.getStream());
    if (error) {
        diagnostics.stage = WeatherFailureStage::Parse;
        captureHeapAfter(diagnostics);
        http.end();
        publishError("Weather response was invalid", diagnostics);
        return;
    }

    JsonVariantConst current = document["current"];
    JsonVariantConst daily = document["daily"];
    if (current.isNull() || daily.isNull()) {
        diagnostics.stage = WeatherFailureStage::Fields;
        captureHeapAfter(diagnostics);
        http.end();
        publishError("Weather fields were missing", diagnostics);
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

void WeatherService::publishError(
    const char* message, const WeatherFailureDiagnostics& diagnostics) {
    WeatherSnapshot result = snapshot();
    result.state = WeatherState::Error;
    result.httpStatus = diagnostics.httpStatus;
    result.failureDiagnostics = diagnostics;
    copyText(result.error, message);
    publish(result);
}

}  // namespace pd
