#include "apps/settings/settings_app_text.h"

#include <cstring>

namespace pd {

const char* localizedResetReasonLabel(const char* reason,
                                      UiLanguage language) {
    if (reason == nullptr) return "";
    if (!isSimplifiedChinese(language)) return reason;
    if (std::strcmp(reason, "power-on") == 0) return "上电";
    if (std::strcmp(reason, "external") == 0) return "外部复位";
    if (std::strcmp(reason, "software") == 0) return "软件重启";
    if (std::strcmp(reason, "panic") == 0) return "崩溃";
    if (std::strcmp(reason, "interrupt-wdt") == 0) return "中断看门狗";
    if (std::strcmp(reason, "task-wdt") == 0) return "任务看门狗";
    if (std::strcmp(reason, "watchdog") == 0) return "看门狗";
    if (std::strcmp(reason, "deep-sleep") == 0) return "深度睡眠唤醒";
    if (std::strcmp(reason, "brownout") == 0) return "电压过低";
    if (std::strcmp(reason, "sdio") == 0) return "SDIO";
    if (std::strcmp(reason, "unknown") == 0) return "未知";
    if (std::strcmp(reason, "other") == 0) return "其他";
    return reason;
}

const char* localizedStorageErrorLabel(const char* error,
                                       UiLanguage language) {
    if (error == nullptr) return "";
    if (!isSimplifiedChinese(language)) return error;
    if (std::strcmp(error, "SD init/format failed") == 0) {
        return "TF 初始化/格式化失败";
    }
    if (std::strcmp(error, "SD SPI init failed") == 0) return "TF SPI 初始化失败";
    if (std::strcmp(error, "No TF card detected") == 0) return "未检测到 TF 卡";
    return error;
}

}  // namespace pd
