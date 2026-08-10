#include "apps/launcher/launcher_app.h"

#include <cstdio>

#include "apps/gps/gps_app_text.h"
#include "apps/weather/weather_app_text.h"
#include "core/gps_data.h"
#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/gps_service.h"
#include "services/weather_service.h"
#include "ui/status_bar.h"
#include "ui/localized_font.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* appTitle(AppId id, UiLanguage language) {
    switch (id) {
        case AppId::Keyboard: return localized(language, "KEYBOARD", "键盘");
        case AppId::Ssh: return localized(language, "SSH TERMINAL", "SSH 终端");
        case AppId::Gps: return localized(language, "GPS", "GPS 定位");
        case AppId::Motion: return localized(language, "MOTION", "运动");
        case AppId::Remote: return localized(language, "REMOTE", "遥控器");
        case AppId::LoRa: return localized(language, "LORA", "LoRa 通信");
        case AppId::Media: return localized(language, "MEDIA", "媒体");
        case AppId::Weather: return localized(language, "WEATHER", "天气");
        case AppId::Settings: return localized(language, "SETTINGS", "设置");
        default: return localized(language, "APP", "应用");
    }
}

const char* weatherStateText(WeatherState state, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return weatherStateLabel(state);
    switch (state) {
        case WeatherState::Idle: return "等待更新";
        case WeatherState::Fetching: return "正在获取";
        case WeatherState::Ready: return "已更新";
        case WeatherState::Error: return "获取失败";
    }
    return "未知";
}

const char* appIcon(AppId id) {
    switch (id) {
        case AppId::Keyboard: return ">_";
        case AppId::Ssh: return "$_";
        case AppId::Gps: return "G+";
        case AppId::Motion: return "IM";
        case AppId::Remote: return "IR";
        case AppId::LoRa: return "LR";
        case AppId::Media: return "MP";
        case AppId::Weather: return "WX";
        case AppId::Settings: return "::";
        default: return "--";
    }
}

}  // namespace

void LauncherApp::onEnter(SystemContext&) {}
void LauncherApp::onExit(SystemContext&) {}
void LauncherApp::update(uint32_t, SystemContext&) {}

void LauncherApp::onInput(const InputEvent& event, SystemContext& context) {
    const AppId requested = model_.handle(event.action);
    if (requested != AppId::None) context.requestApp(requested);
}

void LauncherApp::render(Display& display, const SystemContext& context) {
    auto& canvas = display.canvas();
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display, makeStatusBarData("POCKET DECK", context));

    constexpr int16_t cardX = 52;
    constexpr int16_t cardY = 27;
    constexpr int16_t cardW = 136;
    constexpr int16_t cardH = 76;
    canvas.drawRoundRect(-13, 38, 44, 54, 7, theme::kBorder);
    canvas.drawRoundRect(209, 38, 44, 54, 7, theme::kBorder);
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 8, theme::kPanelRaised);
    canvas.drawRoundRect(cardX, cardY, cardW, cardH, 8, theme::kPrimary);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kPrimary, theme::kPanelRaised);
    setTechnicalFont(canvas);
    canvas.setTextSize(2);
    canvas.drawString(appIcon(model_.selected()), cardX + cardW / 2, cardY + 23);
    setUiFont(canvas, language);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(appTitle(model_.selected(), language), cardX + cardW / 2,
                      cardY + 47);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    char subtitle[80];
    if (model_.selected() == AppId::Keyboard) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      context.bleConnected
                          ? localized(language, "Mac connected", "Mac 已连接")
                          : localized(language, "Ready to pair", "等待配对"));
    } else if (model_.selected() == AppId::Ssh) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      context.wifiConnected
                          ? localized(language, "Remote shell ready", "远程终端就绪")
                          : localized(language, "Wi-Fi required", "需要 Wi-Fi"));
    } else if (model_.selected() == AppId::Gps) {
        const GpsSnapshot gps = context.gps != nullptr ? context.gps->snapshot() : GpsSnapshot{};
        if (classifyGpsState(gps) == GpsState::Fix && gps.satellitesValid) {
            if (isSimplifiedChinese(language)) {
                std::snprintf(subtitle, sizeof(subtitle), "已定位 / %lu 颗卫星",
                              static_cast<unsigned long>(gps.satellites));
            } else {
                std::snprintf(subtitle, sizeof(subtitle), "Fix / %lu satellites",
                              static_cast<unsigned long>(gps.satellites));
            }
        } else {
            std::snprintf(subtitle, sizeof(subtitle), "%s",
                          localizedGpsStateLabel(classifyGpsState(gps), language));
        }
    } else if (model_.selected() == AppId::Motion) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      localized(language, "Accelerometer + gyro",
                                "加速度计 + 陀螺仪"));
    } else if (model_.selected() == AppId::Remote) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      localized(language, "Sony TV control", "Sony 电视遥控"));
    } else if (model_.selected() == AppId::LoRa) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      localized(language, "Raw 868 MHz terminal", "868 MHz 文本终端"));
    } else if (model_.selected() == AppId::Media) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      localized(language, "TF card MP3 player", "TF 卡 MP3 播放器"));
    } else if (model_.selected() == AppId::Weather) {
        const WeatherSnapshot weather = context.weather != nullptr
                                            ? context.weather->snapshot()
                                            : WeatherSnapshot{};
        if (weather.valid && weather.state == WeatherState::Ready) {
            if (isSimplifiedChinese(language)) {
                std::snprintf(subtitle, sizeof(subtitle), "%.1f C / %s",
                              weather.temperatureC,
                              localizedWeatherCodeLabel(weather.weatherCode, language));
            } else {
                std::snprintf(subtitle, sizeof(subtitle), "%.1f C / %s",
                              weather.temperatureC, weatherCodeLabel(weather.weatherCode));
            }
        } else {
            std::snprintf(subtitle, sizeof(subtitle), "%s",
                          weatherStateText(weather.state, language));
        }
    } else {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      localized(language, "System controls", "系统设置与管理"));
    }
    canvas.drawString(subtitle, cardX + cardW / 2, cardY + 62);

    const int16_t hintY = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, hintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_center);
    setUiFont(canvas, language);
    canvas.drawString(localized(language, "FN+, / FN+/  SELECT   ENTER OPEN",
                                "FN+, / FN+/ 切换  ENTER 打开"),
                      config::kScreenWidth / 2,
                      hintY + theme::kHintHeight / 2);
}

}  // namespace pd
