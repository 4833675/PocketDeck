#include "apps/weather/weather_app.h"
#include "apps/weather/weather_app_text.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/gps_data.h"
#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "core/weather_data.h"
#include "core/wifi_data.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/gps_service.h"
#include "services/weather_service.h"
#include "services/wifi_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

constexpr uint32_t kRefreshIntervalMs = 15u * 60u * 1000u;

void drawHint(M5Canvas& canvas, UiLanguage language) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(localized(language,
                                "ENTER REFRESH   DEL HOME   DATA OPEN-METEO",
                                "ENTER 刷新   DEL 返回   OPEN-METEO"),
                      config::kScreenWidth / 2, y + theme::kHintHeight / 2);
}

void drawCenteredState(M5Canvas& canvas, const char* title, const char* detail,
                       uint16_t color, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(isSimplifiedChinese(language) ? 1 : 2);
    canvas.setTextColor(color, theme::kBackground);
    canvas.drawString(title, config::kScreenWidth / 2, 48);
    setFontForText(canvas, detail);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(detail, config::kScreenWidth / 2, 75);
}

void formatObservationClock(char (&output)[6], const WeatherSnapshot& weather) {
    std::snprintf(output, sizeof(output), "--:--");
    const char* separator = std::strchr(weather.observationTime.data(), 'T');
    if (separator != nullptr && std::strlen(separator + 1) >= 5) {
        std::snprintf(output, sizeof(output), "%.5s", separator + 1);
    }
}

void formatAge(char* output, std::size_t outputSize, uint32_t ageMs,
               UiLanguage language) {
    const uint32_t minutes = ageMs / 60000u;
    if (minutes < 60u) {
        std::snprintf(output, outputSize,
                      localized(language, "%lum", "%lu分"),
                      static_cast<unsigned long>(minutes));
    } else if (minutes < 48u * 60u) {
        std::snprintf(output, outputSize,
                      localized(language, "%luh", "%lu时"),
                      static_cast<unsigned long>(minutes / 60u));
    } else {
        std::snprintf(output, outputSize,
                      localized(language, "%lud", "%lu天"),
                      static_cast<unsigned long>(minutes / (24u * 60u)));
    }
}

uint16_t weatherStatusColor(WeatherDisplayState state) {
    if (state == WeatherDisplayState::Updating) return theme::kPrimary;
    if (state == WeatherDisplayState::CachedError) return theme::kError;
    if (state == WeatherDisplayState::CachedNoGps ||
        state == WeatherDisplayState::CachedOffline) {
        return theme::kWarning;
    }
    return theme::kMuted;
}

void drawWeatherData(M5Canvas& canvas, const WeatherSnapshot& weather,
                     WeatherDisplayState displayState, uint32_t nowMs,
                     UiLanguage language) {
    char line[96];
    canvas.setTextDatum(top_left);
    setTechnicalFont(canvas);
    canvas.setTextSize(2);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    std::snprintf(line, sizeof(line), "%.1f C", weather.temperatureC);
    canvas.drawString(line, 8, 24);
    setUiFont(canvas, language);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString(localizedWeatherCodeLabel(weather.weatherCode, language), 111, 29);
    if (isSimplifiedChinese(language)) {
        std::snprintf(line, sizeof(line), "体感 %.1f C  湿度 %u%%", weather.apparentC,
                      static_cast<unsigned>(weather.humidityPercent));
        canvas.drawString(line, 8, 55);
        std::snprintf(line, sizeof(line), "风速 %.1fkm/h  高 %.1f  低 %.1f",
                      weather.windKph, weather.highC, weather.lowC);
        canvas.drawString(line, 8, 72);
        std::snprintf(line, sizeof(line), "日出 %s  日落 %s", weather.sunrise.data(),
                      weather.sunset.data());
        canvas.drawString(line, 8, 89);
    } else {
        setTechnicalFont(canvas);
        std::snprintf(line, sizeof(line),
                      "FEELS %.1f C   HUM %u%%   WIND %.1f km/h",
                      weather.apparentC,
                      static_cast<unsigned>(weather.humidityPercent),
                      weather.windKph);
        canvas.drawString(line, 8, 55);
        std::snprintf(line, sizeof(line), "TODAY  HIGH %.1f C   LOW %.1f C",
                      weather.highC, weather.lowC);
        canvas.drawString(line, 8, 72);
        std::snprintf(line, sizeof(line), "SUNRISE %s        SUNSET %s",
                      weather.sunrise.data(), weather.sunset.data());
        canvas.drawString(line, 8, 89);
    }

    char observed[6];
    formatObservationClock(observed, weather);
    char age[16];
    formatAge(age, sizeof(age), nowMs - weather.updatedAtMs, language);
    const char* status = localizedWeatherDisplayLabel(WeatherDisplayState::Live,
                                                      language);
    char cachedStatus[48];
    if (displayState == WeatherDisplayState::Updating) {
        status = localizedWeatherDisplayLabel(displayState, language);
    } else if (displayState == WeatherDisplayState::CachedNoGps) {
        std::snprintf(cachedStatus, sizeof(cachedStatus),
                      localized(language, "CACHED %s GPS--", "缓存 %s GPS--"), age);
        status = cachedStatus;
    } else if (displayState == WeatherDisplayState::CachedOffline) {
        std::snprintf(cachedStatus, sizeof(cachedStatus),
                      localized(language, "CACHED %s WIFI--", "缓存 %s WiFi--"), age);
        status = cachedStatus;
    } else if (displayState == WeatherDisplayState::CachedError) {
        std::snprintf(cachedStatus, sizeof(cachedStatus),
                      localized(language, "CACHED %s ERROR", "缓存 %s 错误"), age);
        status = cachedStatus;
    }
    std::snprintf(line, sizeof(line), localized(language, "AS OF %s  %s", "观测 %s  %s"),
                  observed, status);
    setUiFont(canvas, language);
    canvas.setTextColor(weatherStatusColor(displayState), theme::kBackground);
    canvas.drawString(line, 8, 104);
}

}  // namespace

void WeatherApp::onEnter(SystemContext&) {
    refreshPending_ = true;
}

void WeatherApp::onExit(SystemContext&) {}

void WeatherApp::onInput(const InputEvent& event, SystemContext& context) {
    if (event.action == InputAction::Confirm) {
        refreshPending_ = true;
    } else if (event.action == InputAction::Back) {
        context.requestApp(AppId::Launcher);
    }
}

void WeatherApp::update(uint32_t nowMs, SystemContext& context) {
    if (context.weather == nullptr || context.wifi == nullptr || context.gps == nullptr) return;
    const WifiSnapshot wifi = context.wifi->snapshot();
    const GpsSnapshot gps = context.gps->snapshot();
    if (!wifi.connected || !gps.locationValid || gps.locationAgeMs > 5000) return;

    const WeatherSnapshot weather = context.weather->snapshot();
    const bool stale = weather.state == WeatherState::Ready &&
                       nowMs - weather.updatedAtMs >= kRefreshIntervalMs;
    const bool moved = weather.valid &&
                       (std::fabs(weather.latitude - gps.latitude) > 0.05 ||
                        std::fabs(weather.longitude - gps.longitude) > 0.05);
    const bool needsInitial = weather.state == WeatherState::Idle;
    if (refreshPending_ || stale || moved || needsInitial) {
        if (context.weather->requestRefresh(gps.latitude, gps.longitude)) {
            refreshPending_ = false;
        }
    }
}

void WeatherApp::render(Display& display, const SystemContext& context) {
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display,
                  makeStatusBarData(localized(language, "WEATHER", "天气"), context));
    auto& canvas = display.canvas();
    const WifiSnapshot wifi = context.wifi != nullptr ? context.wifi->snapshot()
                                                       : WifiSnapshot{};
    const GpsSnapshot gps = context.gps != nullptr ? context.gps->snapshot() : GpsSnapshot{};
    const WeatherSnapshot weather = context.weather != nullptr
                                        ? context.weather->snapshot()
                                        : WeatherSnapshot{};
    const bool gpsFresh = gps.locationValid && gps.locationAgeMs <= 5000;
    const WeatherDisplayState displayState = classifyWeatherDisplay(
        weather, wifi.enabled, wifi.connected, gpsFresh);

    if (weatherDisplayShowsData(displayState)) {
        drawWeatherData(canvas, weather, displayState, context.uptimeMs, language);
    } else if (displayState == WeatherDisplayState::WifiOff) {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localized(language, "Enable Wi-Fi in Settings",
                                    "请在设置中开启 Wi-Fi"),
                          theme::kError, language);
    } else if (displayState == WeatherDisplayState::NoNetwork) {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localizedWeatherWifiStateLabel(wifi.state, language),
                          theme::kWarning, language);
    } else if (displayState == WeatherDisplayState::WaitingGps) {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localized(language, "Weather uses your live position",
                                    "天气需要当前位置"),
                          theme::kWarning, language);
    } else if (displayState == WeatherDisplayState::Fetching) {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localized(language, "Downloading local forecast",
                                    "正在下载本地天气"),
                          theme::kPrimary, language);
    } else if (displayState == WeatherDisplayState::Error) {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localizedWeatherErrorLabel(weather.error.data(), language),
                          theme::kError, language);
    } else {
        drawCenteredState(canvas, localizedWeatherDisplayLabel(displayState, language),
                          localized(language, "Press Enter to refresh", "按 Enter 刷新"),
                          theme::kMuted, language);
    }
    drawHint(canvas, language);
}

}  // namespace pd
