#include "apps/gps/gps_app_text.h"

#include <cmath>

namespace pd {

const char* localizedGpsStateLabel(GpsState state, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return gpsStateLabel(state);
    switch (state) {
        case GpsState::NoData: return "无数据";
        case GpsState::NoStream: return "数据中断";
        case GpsState::Searching: return "正在搜星";
        case GpsState::Stale: return "定位过期";
        case GpsState::Fix: return "已定位";
    }
    return "未知";
}

const char* localizedGpsCompassPoint(double courseDegrees, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return gpsCompassPoint(courseDegrees);
    if (!std::isfinite(courseDegrees)) return "--";
    static constexpr const char* kPoints[] = {
        "北", "东北", "东", "东南", "南", "西南", "西", "西北",
    };
    double normalized = std::fmod(courseDegrees, 360.0);
    if (normalized < 0.0) normalized += 360.0;
    const int index = static_cast<int>((normalized + 22.5) / 45.0) % 8;
    return kPoints[index];
}

const char* localizedGpsFixQualityLabel(char quality, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return gpsFixQualityLabel(quality);
    switch (quality) {
        case '0': return "无";
        case '1': return "GPS";
        case '2': return "DGPS";
        case '3': return "PPS";
        case '4': return "RTK";
        case '5': return "浮点";
        case '6': return "估算";
        case '7': return "手动";
        case '8': return "模拟";
        default: return "--";
    }
}

const char* localizedGpsFixModeLabel(char mode, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return gpsFixModeLabel(mode);
    switch (mode) {
        case 'N': return "无";
        case 'A': return "自主";
        case 'D': return "差分";
        case 'E': return "估算";
        default: return "--";
    }
}

}  // namespace pd
