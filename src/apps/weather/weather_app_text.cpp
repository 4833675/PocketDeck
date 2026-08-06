#include "apps/weather/weather_app_text.h"

#include <cstring>

namespace pd {

const char* localizedWeatherCodeLabel(uint8_t code, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return weatherCodeLabel(code);
    if (code == 0) return "晴";
    if (code == 1) return "大部晴朗";
    if (code == 2) return "局部多云";
    if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 57) return "毛毛雨";
    if (code >= 61 && code <= 67) return "雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code == 85 || code == 86) return "阵雪";
    if (code >= 95) return "雷暴";
    return "未知";
}

const char* localizedWeatherDisplayLabel(WeatherDisplayState state,
                                         UiLanguage language) {
    if (!isSimplifiedChinese(language)) {
        switch (state) {
            case WeatherDisplayState::Live: return "LIVE";
            case WeatherDisplayState::Updating: return "UPDATING";
            case WeatherDisplayState::CachedNoGps:
            case WeatherDisplayState::CachedOffline:
            case WeatherDisplayState::CachedError: return "CACHED";
            case WeatherDisplayState::WifiOff: return "WI-FI OFF";
            case WeatherDisplayState::NoNetwork: return "NO NETWORK";
            case WeatherDisplayState::WaitingGps: return "WAITING FOR GPS";
            case WeatherDisplayState::Fetching: return "FETCHING";
            case WeatherDisplayState::Error: return "WEATHER ERROR";
            case WeatherDisplayState::ReadyToFetch: return "READY TO FETCH";
        }
        return "UNKNOWN";
    }

    switch (state) {
        case WeatherDisplayState::Live: return "实时";
        case WeatherDisplayState::Updating: return "更新中";
        case WeatherDisplayState::CachedNoGps:
        case WeatherDisplayState::CachedOffline:
        case WeatherDisplayState::CachedError: return "缓存";
        case WeatherDisplayState::WifiOff: return "Wi-Fi 已关闭";
        case WeatherDisplayState::NoNetwork: return "网络未连接";
        case WeatherDisplayState::WaitingGps: return "等待 GPS 定位";
        case WeatherDisplayState::Fetching: return "正在获取";
        case WeatherDisplayState::Error: return "天气错误";
        case WeatherDisplayState::ReadyToFetch: return "可以获取天气";
    }
    return "未知";
}

const char* localizedWeatherWifiStateLabel(WifiState state,
                                           UiLanguage language) {
    if (!isSimplifiedChinese(language)) return wifiStateLabel(state);
    switch (state) {
        case WifiState::Disabled: return "已关闭";
        case WifiState::Idle: return "等待连接";
        case WifiState::Scanning: return "正在扫描";
        case WifiState::Connecting: return "正在连接";
        case WifiState::Connected: return "已连接";
        case WifiState::Error: return "连接错误";
    }
    return "未知";
}

const char* localizedWeatherErrorLabel(const char* error,
                                       UiLanguage language) {
    if (error == nullptr) return "";
    if (!isSimplifiedChinese(language)) return error;
    if (std::strcmp(error, "Wi-Fi is not connected") == 0) return "Wi-Fi 未连接";
    if (std::strcmp(error, "Unable to start weather task") == 0) {
        return "无法启动天气任务";
    }
    if (std::strcmp(error, "Wi-Fi disconnected") == 0) return "Wi-Fi 已断开";
    if (std::strcmp(error, "Unable to open weather endpoint") == 0) {
        return "无法连接天气服务";
    }
    if (std::strcmp(error, "Weather HTTP request failed") == 0) {
        return "天气请求失败";
    }
    if (std::strcmp(error, "Weather response was invalid") == 0) {
        return "天气数据无效";
    }
    if (std::strcmp(error, "Weather fields were missing") == 0) {
        return "天气字段缺失";
    }
    return error;
}

}  // namespace pd
