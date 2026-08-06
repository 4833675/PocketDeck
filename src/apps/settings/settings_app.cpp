#include "apps/settings/settings_app.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "apps/settings/settings_app_text.h"
#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "core/wifi_data.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/ble_keyboard_service.h"
#include "services/diagnostics_service.h"
#include "services/sd_log_service.h"
#include "services/wifi_service.h"
#include "ui/status_bar.h"
#include "ui/localized_font.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* bleStateName(BleKeyboardState state, UiLanguage language) {
    switch (state) {
        case BleKeyboardState::Disabled: return localized(language, "OFF", "关闭");
        case BleKeyboardState::Advertising:
            return localized(language, "ADVERTISING", "广播中");
        case BleKeyboardState::Pairing: return localized(language, "PAIRING", "配对中");
        case BleKeyboardState::Connected:
            return localized(language, "CONNECTED", "已连接");
        case BleKeyboardState::Error: return localized(language, "ERROR", "错误");
    }
    return localized(language, "UNKNOWN", "未知");
}

const char* wifiStateName(WifiState state, UiLanguage language) {
    if (!isSimplifiedChinese(language)) return wifiStateLabel(state);
    switch (state) {
        case WifiState::Disabled: return "关闭";
        case WifiState::Idle: return "空闲";
        case WifiState::Scanning: return "扫描中";
        case WifiState::Connecting: return "连接中";
        case WifiState::Connected: return "已连接";
        case WifiState::Error: return "错误";
    }
    return "未知";
}

uint16_t wifiStateColor(const WifiSnapshot& wifi) {
    if (!wifi.enabled || wifi.state == WifiState::Disabled) return theme::kError;
    if (wifi.connected) return theme::kPrimary;
    if (wifi.state == WifiState::Error) return theme::kError;
    return theme::kWarning;
}

const char* sdStateLabel(SdLogState state, UiLanguage language) {
    switch (state) {
        case SdLogState::Unavailable: return localized(language, "UNAVAILABLE", "不可用");
        case SdLogState::Ready: return localized(language, "LOGGING", "记录中");
        case SdLogState::Formatting: return localized(language, "FORMATTING", "格式化中");
        case SdLogState::Error: return localized(language, "ERROR", "错误");
    }
    return localized(language, "UNKNOWN", "未知");
}

uint16_t sdStateColor(SdLogState state) {
    if (state == SdLogState::Ready) return theme::kPrimary;
    if (state == SdLogState::Error) return theme::kError;
    return theme::kWarning;
}

void drawHint(M5Canvas& canvas, UiLanguage language, const char* english,
              const char* simplifiedChinese) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(localized(language, english, simplifiedChinese),
                      config::kScreenWidth / 2, y + theme::kHintHeight / 2);
}

void drawChoice(M5Canvas& canvas, int16_t y, const char* label, bool selected) {
    const uint16_t background = selected ? theme::kPanelRaised : theme::kBackground;
    canvas.fillRoundRect(7, y, 103, 18, 4, background);
    if (selected) canvas.drawRoundRect(7, y, 103, 18, 4, theme::kPrimary);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(selected ? theme::kText : theme::kMuted, background);
    canvas.drawString(label, 13, y + 9);
}

void compactSsid(char* output, std::size_t outputSize, const char* ssid,
                 std::size_t maxCharacters) {
    if (outputSize == 0) return;
    if (ssid == nullptr || ssid[0] == '\0') {
        std::snprintf(output, outputSize, "--");
        return;
    }
    std::snprintf(output, outputSize, "%.*s", static_cast<int>(maxCharacters), ssid);
}

void drawCategories(M5Canvas& canvas, const SettingsModel& model,
                    const SystemContext& context, const WifiSnapshot& wifi,
                    const BleKeyboardSnapshot& ble, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.fillRoundRect(5, 23, 77, 88, 5, theme::kPanel);
    const char* labels[] = {
        localized(language, "WI-FI", "无线网络"),
        localized(language, "BLUETOOTH", "蓝牙"),
        localized(language, "SYSTEM", "系统"),
    };
    for (uint8_t index = 0; index < 3; ++index) {
        const bool selected = static_cast<uint8_t>(model.category()) == index;
        const int16_t y = 27 + static_cast<int16_t>(index) * 28;
        canvas.fillRoundRect(10, y, 67, 22, 4,
                             selected ? theme::kPanelRaised : theme::kPanel);
        if (selected) canvas.drawRoundRect(10, y, 67, 22, 4, theme::kPrimary);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(selected ? theme::kText : theme::kMuted,
                            selected ? theme::kPanelRaised : theme::kPanel);
        canvas.drawString(labels[index], 15, y + 11);
    }

    canvas.setTextDatum(top_left);
    char line[48];
    if (model.category() == SettingsCategory::Wifi) {
        canvas.setTextColor(wifiStateColor(wifi), theme::kBackground);
        canvas.drawString(localized(language, "WI-FI", "无线网络"), 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "%s  %s",
                      localized(language, "State", "状态"),
                      wifiStateName(wifi.state, language));
        canvas.drawString(line, 91, 45);
        char ssid[24];
        compactSsid(ssid, sizeof(ssid), wifi.ssid.data(), 18);
        canvas.drawString(ssid, 91, 59);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        if (wifi.connected) {
            std::snprintf(line, sizeof(line), "%s  %ld dBm", wifi.ip.data(),
                          static_cast<long>(wifi.rssi));
            canvas.drawString(line, 91, 76);
            canvas.drawString(wifi.ntpSynced
                                  ? localized(language, "NTP synchronized", "时间已同步")
                                  : localized(language, "Waiting for NTP", "等待时间同步"),
                              91, 92);
        } else {
            canvas.drawString(wifi.hasSavedNetwork
                                  ? localized(language, "Saved network available", "有已存网络")
                                  : localized(language, "No saved network", "没有已存网络"),
                              91, 76);
            canvas.drawString(localized(language, "Scan to connect", "扫描并连接"), 91, 92);
        }
    } else if (model.category() == SettingsCategory::Bluetooth) {
        canvas.setTextColor(theme::kPrimary, theme::kBackground);
        canvas.drawString(localized(language, "BLUETOOTH", "蓝牙"), 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "%s  %s",
                      localized(language, "State", "状态"),
                      bleStateName(ble.state, language));
        canvas.drawString(line, 91, 45);
        canvas.drawString(context.settings != nullptr ? context.settings->deviceName.data()
                                                       : "Pocket Deck",
                          91, 59);
        std::snprintf(line, sizeof(line), "%s   %s",
                      localized(language, "Host", "主机"),
                      context.settings != nullptr ? context.settings->hostLabel.data() : "Mac");
        canvas.drawString(line, 91, 73);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(ble.bonded
                              ? localized(language, "One host paired", "已配对一台主机")
                              : localized(language, "No paired host", "没有配对主机"),
                          91, 90);
    } else {
        canvas.setTextColor(theme::kPrimary, theme::kBackground);
        canvas.drawString(localized(language, "SYSTEM", "系统"), 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "%s %s",
                      localized(language, "Version", "版本"), config::kFirmwareVersion);
        canvas.drawString(line, 91, 45);
        std::snprintf(line, sizeof(line), "%s   %s",
                      localized(language, "Reset", "重启原因"),
                      localizedResetReasonLabel(context.resetReason, language));
        canvas.drawString(line, 91, 59);
        std::snprintf(line, sizeof(line), "%s    %lu KB",
                      localized(language, "Heap", "内存"),
                      static_cast<unsigned long>(context.freeHeap / 1024u));
        canvas.drawString(line, 91, 73);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Diagnostics & reset", "诊断与重置"), 91, 90);
    }
    drawHint(canvas, language, "FN+;/. MOVE   ENTER OPEN   DEL HOME",
             "FN+;/. 移动  ENTER 打开  DEL 主页");
}

void drawWifi(M5Canvas& canvas, const SettingsModel& model, const WifiSnapshot& wifi,
              UiLanguage language) {
    setUiFont(canvas, language);
    drawChoice(canvas, 22,
               wifi.enabled ? localized(language, "Wi-Fi  ON", "Wi-Fi  开")
                            : localized(language, "Wi-Fi  OFF", "Wi-Fi  关"),
               model.selectedRow() == 0);
    drawChoice(canvas, 44, localized(language, "Scan networks", "扫描网络"),
               model.selectedRow() == 1);
    drawChoice(canvas, 66, localized(language, "Saved networks", "已存网络"),
               model.selectedRow() == 2);
    drawChoice(canvas, 88, localized(language, "Network info", "网络信息"),
               model.selectedRow() == 3);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(wifiStateColor(wifi), theme::kBackground);
    canvas.drawString(wifiStateName(wifi.state, language), 121, 23);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[40];
    char ssid[22];
    compactSsid(ssid, sizeof(ssid), wifi.ssid.data(), 17);
    canvas.drawString(ssid, 121, 41);
    if (wifi.connected) {
        std::snprintf(line, sizeof(line), "%ld dBm", static_cast<long>(wifi.rssi));
        canvas.drawString(line, 121, 58);
        canvas.drawString(wifi.ip.data(), 121, 74);
    } else {
        canvas.drawString(wifi.hasSavedNetwork
                              ? localized(language, "Saved", "已有记录")
                              : localized(language, "No credentials", "没有凭据"),
                          121, 58);
        canvas.drawString(localized(language, "Not connected", "未连接"), 121, 74);
    }
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(wifi.ntpSynced
                          ? localized(language, "NTP ready", "时间已同步")
                          : "NTP --",
                      121, 92);
    drawHint(canvas, language, "FN+;/. MOVE   ENTER SELECT   DEL BACK",
             "FN+;/. 移动  ENTER 选择  DEL 返回");
}

void drawWifiNetworks(M5Canvas& canvas, const SettingsModel& model,
                      const WifiSnapshot& wifi, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    if (!wifi.enabled) {
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString(localized(language, "WI-FI IS OFF", "Wi-Fi 已关闭"), 8, 27);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Enable it on the previous page",
                                    "请在上一页开启"),
                          8, 47);
    } else if (wifi.state == WifiState::Scanning && wifi.networkCount == 0) {
        canvas.setTextColor(theme::kWarning, theme::kBackground);
        canvas.drawString(localized(language, "SCANNING NETWORKS...", "正在扫描网络..."),
                          8, 27);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "BLE remains available", "蓝牙仍然可用"),
                          8, 47);
    } else if (wifi.networkCount == 0) {
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "No networks found", "没有发现网络"), 8, 27);
        canvas.drawString(localized(language, "Press TAB to scan again", "按 TAB 再次扫描"),
                          8, 47);
    } else {
        setTechnicalFont(canvas);
        const uint8_t selected = std::min<uint8_t>(model.selectedRow(), wifi.networkCount - 1);
        const uint8_t start = selected >= 5 ? selected - 4 : 0;
        const uint8_t end = std::min<uint8_t>(wifi.networkCount, start + 5);
        for (uint8_t index = start; index < end; ++index) {
            const bool active = index == selected;
            const int16_t y = 23 + static_cast<int16_t>(index - start) * 18;
            const uint16_t background = active ? theme::kPanelRaised : theme::kBackground;
            canvas.fillRoundRect(6, y, 228, 16, 3, background);
            if (active) canvas.drawRoundRect(6, y, 228, 16, 3, theme::kPrimary);
            char line[52];
            std::snprintf(line, sizeof(line), "%c%c %-22.22s %4ld",
                          wifi.networks[index].saved ? 'S' : ' ',
                          wifi.networks[index].secured ? '*' : 'o',
                          wifi.networks[index].ssid.data(),
                          static_cast<long>(wifi.networks[index].rssi));
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(active ? theme::kText : theme::kMuted, background);
            canvas.drawString(line, 11, y + 8);
        }
    }
    drawHint(canvas, language, "S=SAVED  ENTER CONNECT  TAB SCAN  DEL BACK",
             "ENTER连接  TAB扫描  DEL返回");
}

void drawWifiSavedNetworks(M5Canvas& canvas, const SettingsModel& model,
                           const WifiSnapshot& wifi, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    if (wifi.savedNetworkCount == 0) {
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "No saved networks", "没有已存网络"), 8, 27);
        canvas.drawString(localized(language, "Connect from Scan networks",
                                    "请先扫描并连接"),
                          8, 47);
    } else {
        setTechnicalFont(canvas);
        const uint8_t selected =
            std::min<uint8_t>(model.selectedRow(), wifi.savedNetworkCount - 1);
        const uint8_t start = selected >= 5 ? selected - 4 : 0;
        const uint8_t end = std::min<uint8_t>(wifi.savedNetworkCount, start + 5);
        for (uint8_t index = start; index < end; ++index) {
            const bool active = index == selected;
            const int16_t y = 23 + static_cast<int16_t>(index - start) * 18;
            const uint16_t background = active ? theme::kPanelRaised : theme::kBackground;
            canvas.fillRoundRect(6, y, 228, 16, 3, background);
            if (active) canvas.drawRoundRect(6, y, 228, 16, 3, theme::kPrimary);
            char line[44];
            std::snprintf(line, sizeof(line), "%u  %.28s",
                          static_cast<unsigned>(index + 1),
                          wifi.savedNetworks[index].ssid.data());
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(active ? theme::kText : theme::kMuted, background);
            canvas.drawString(line, 11, y + 8);
        }
    }
    drawHint(canvas, language, "FN+;/. MOVE  ENTER FORGET  DEL BACK",
             "FN+;/. 移动  ENTER 忘记  DEL 返回");
}

void drawWifiPassword(M5Canvas& canvas, const char* ssid, const char* password,
                      UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    canvas.drawString(localized(language, "WI-FI PASSWORD", "Wi-Fi 密码"), 8, 25);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char network[72];
    std::snprintf(network, sizeof(network), "%s: %.27s",
                  localized(language, "Network", "网络"), ssid);
    canvas.drawString(network, 8, 43);

    const std::size_t length = std::strlen(password);
    char mask[33];
    const std::size_t shown = std::min<std::size_t>(length, sizeof(mask) - 1);
    std::memset(mask, '*', shown);
    mask[shown] = '\0';
    canvas.fillRoundRect(7, 61, 226, 24, 4, theme::kPanelRaised);
    canvas.drawRoundRect(7, 61, 226, 24, 4, theme::kPrimary);
    canvas.setTextDatum(middle_left);
    setTechnicalFont(canvas);
    canvas.drawString(mask, 13, 73);

    setUiFont(canvas, language);
    char count[48];
    if (isSimplifiedChinese(language)) {
        std::snprintf(count, sizeof(count), "已输入 %u 个字符",
                      static_cast<unsigned>(length));
    } else {
        std::snprintf(count, sizeof(count), "%u characters", static_cast<unsigned>(length));
    }
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(count, 8, 91);
    if (isSimplifiedChinese(language)) {
        canvas.drawString("仅保存在本机", 8, 104);
    } else {
        canvas.drawString("Saved only on this device", 105, 91);
    }
    drawHint(canvas, language, "TYPE  ENTER CONNECT  DEL ERASE  FN+` CANCEL",
             "输入  ENTER 连接  DEL 删除  FN+` 取消");
}

void drawWifiDiagnostics(M5Canvas& canvas, const WifiSnapshot& wifi,
                         UiLanguage language) {
    canvas.setTextDatum(top_left);
    char line[64];
    canvas.setTextColor(wifiStateColor(wifi), theme::kBackground);
    if (isSimplifiedChinese(language)) {
        setUiFont(canvas, language);
        std::snprintf(line, sizeof(line), "状态 %s  代码 %d",
                      wifiStateName(wifi.state, language),
                      static_cast<int>(wifi.lastStatus));
    } else {
        setTechnicalFont(canvas);
        std::snprintf(line, sizeof(line), "STATE %-11s STATUS %d",
                      wifiStateLabel(wifi.state), static_cast<int>(wifi.lastStatus));
    }
    canvas.drawString(line, 7, 23);
    setTechnicalFont(canvas);
    canvas.setTextColor(theme::kText, theme::kBackground);
    std::snprintf(line, sizeof(line), "SSID  %.27s", wifi.ssid.data());
    canvas.drawString(line, 7, 39);
    std::snprintf(line, sizeof(line), "IP    %-15s RSSI %ld", wifi.ip.data(),
                  static_cast<long>(wifi.rssi));
    canvas.drawString(line, 7, 55);
    std::snprintf(line, sizeof(line), "GW    %s", wifi.gateway.data());
    canvas.drawString(line, 7, 71);
    std::snprintf(line, sizeof(line), "DNS   %s", wifi.dns.data());
    canvas.drawString(line, 7, 87);

    canvas.setTextColor(theme::kMuted, theme::kBackground);
    if (wifi.ntpSynced && wifi.utcEpoch > 0) {
        const std::time_t epoch = static_cast<std::time_t>(wifi.utcEpoch);
        std::tm utc{};
        gmtime_r(&epoch, &utc);
        std::snprintf(line, sizeof(line), "NTP UTC %04d-%02d-%02d %02d:%02d:%02d",
                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                      utc.tm_min, utc.tm_sec);
    } else {
        std::snprintf(line, sizeof(line), "NTP UTC --");
    }
    canvas.drawString(line, 7, 103);
    drawHint(canvas, language, "DEL BACK", "DEL 返回");
}

void drawBluetooth(M5Canvas& canvas, const SettingsModel& model,
                   const SystemContext& context, const BleKeyboardSnapshot& ble,
                   UiLanguage language) {
    setUiFont(canvas, language);
    drawChoice(canvas, 27,
               ble.enabled ? localized(language, "Bluetooth  ON", "蓝牙  开")
                           : localized(language, "Bluetooth  OFF", "蓝牙  关"),
               model.selectedRow() == 0);
    drawChoice(canvas, 50, localized(language, "Disconnect", "断开连接"),
               model.selectedRow() == 1);
    drawChoice(canvas, 73, localized(language, "Forget host", "忘记主机"),
               model.selectedRow() == 2);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    canvas.drawString(bleStateName(ble.state, language), 121, 28);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString(context.settings != nullptr ? context.settings->deviceName.data()
                                                   : "Pocket Deck",
                      121, 47);
    char host[64];
    std::snprintf(host, sizeof(host), "%s: %s",
                  localized(language, "Host", "主机"),
                  context.settings != nullptr ? context.settings->hostLabel.data() : "Mac");
    canvas.drawString(host, 121, 62);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(ble.encrypted
                          ? localized(language, "Encrypted", "已加密")
                          : localized(language, "Not encrypted", "未加密"),
                      121, 79);
    canvas.drawString(ble.bonded
                          ? localized(language, "Bond stored", "已保存配对")
                          : localized(language, "Ready to pair", "等待配对"),
                      121, 94);
    drawHint(canvas, language, "FN+;/. MOVE   ENTER SELECT   DEL BACK",
             "FN+;/. 移动  ENTER 选择  DEL 返回");
}

void drawSystem(M5Canvas& canvas, const SettingsModel& model, const SystemContext& context,
                UiLanguage language) {
    setUiFont(canvas, language);
    drawChoice(canvas, 18,
               isSimplifiedChinese(language) ? "语言  中文" : "Language EN",
               model.selectedRow() == 0);
    drawChoice(canvas, 37, localized(language, "Diagnostics", "诊断信息"),
               model.selectedRow() == 1);
    drawChoice(canvas, 56, localized(language, "TF card logs", "TF 卡日志"),
               model.selectedRow() == 2);
    drawChoice(canvas, 75, localized(language, "Restart", "重新启动"),
               model.selectedRow() == 3);
    drawChoice(canvas, 94, localized(language, "Factory reset", "恢复出厂"),
               model.selectedRow() == 4);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[48];
    std::snprintf(line, sizeof(line), "v%s", config::kFirmwareVersion);
    canvas.drawString(line, 121, 20);
    std::snprintf(line, sizeof(line), "%s %lus",
                  localized(language, "Up", "运行"),
                  static_cast<unsigned long>(context.uptimeMs / 1000u));
    canvas.drawString(line, 121, 37);
    const SdLogSnapshot storage = context.sdLog != nullptr ? context.sdLog->snapshot()
                                                           : SdLogSnapshot{};
    canvas.setTextColor(sdStateColor(storage.state), theme::kBackground);
    std::snprintf(line, sizeof(line), "TF %s", sdStateLabel(storage.state, language));
    canvas.drawString(line, 121, 54);
    canvas.setTextColor(theme::kText, theme::kBackground);
    std::snprintf(line, sizeof(line), "%s %lu KB",
                  localized(language, "Heap", "内存"),
                  static_cast<unsigned long>(context.freeHeap / 1024u));
    canvas.drawString(line, 121, 71);
    std::snprintf(line, sizeof(line), "%s  %lu KB",
                  localized(language, "Min", "最低"),
                  static_cast<unsigned long>(context.minimumFreeHeap / 1024u));
    canvas.drawString(line, 121, 88);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(localizedResetReasonLabel(context.resetReason, language), 121, 103);
    drawHint(canvas, language, "FN+;/. MOVE   ENTER SELECT   DEL BACK",
             "FN+;/. 移动  ENTER 选择  DEL 返回");
}

void drawStorage(M5Canvas& canvas, const SettingsModel& model,
                 const SystemContext& context, UiLanguage language) {
    setUiFont(canvas, language);
    drawChoice(canvas, 28, localized(language, "Mount / retry", "挂载 / 重试"),
               model.selectedRow() == 0);
    drawChoice(canvas, 54, localized(language, "Format TF card", "格式化 TF 卡"),
               model.selectedRow() == 1);

    const SdLogSnapshot storage = context.sdLog != nullptr ? context.sdLog->snapshot()
                                                           : SdLogSnapshot{};
    canvas.setTextDatum(top_left);
    canvas.setTextColor(sdStateColor(storage.state), theme::kBackground);
    canvas.drawString(sdStateLabel(storage.state, language), 121, 29);

    char line[52];
    canvas.setTextColor(theme::kText, theme::kBackground);
    if (storage.cardBytes > 0) {
        std::snprintf(line, sizeof(line), "%s %llu MB",
                      localized(language, "Size", "容量"),
                      static_cast<unsigned long long>(storage.cardBytes / (1024u * 1024u)));
    } else {
        std::snprintf(line, sizeof(line), "%s --",
                      localized(language, "Size", "容量"));
    }
    canvas.drawString(line, 121, 47);
    std::snprintf(line, sizeof(line), "%s %lu",
                  localized(language, "Lines", "日志行"),
                  static_cast<unsigned long>(storage.linesWritten));
    canvas.drawString(line, 121, 64);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString("/PocketDeck/", 121, 81);
    canvas.drawString("system.log", 121, 96);
    if (storage.state != SdLogState::Ready && storage.error[0] != '\0') {
        canvas.setTextColor(theme::kWarning, theme::kBackground);
        const char* error = localizedStorageErrorLabel(storage.error.data(), language);
        setFontForText(canvas, error);
        canvas.drawString(error, 7, 103);
    }
    drawHint(canvas, language, "FORMAT ERASES CARD   ENTER SELECT   DEL BACK",
             "格式化会清卡  ENTER确认  DEL返回");
}

void drawDiagnostics(M5Canvas& canvas, const SystemContext& context,
                     UiLanguage language) {
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[64];
    if (isSimplifiedChinese(language)) {
        setUiFont(canvas, language);
        std::snprintf(line, sizeof(line), "重启 %s  内存 %luK",
                      localizedResetReasonLabel(context.resetReason, language),
                      static_cast<unsigned long>(context.freeHeap / 1024u));
    } else {
        setTechnicalFont(canvas);
        std::snprintf(line, sizeof(line), "RESET %-10s  HEAP %luK", context.resetReason,
                      static_cast<unsigned long>(context.freeHeap / 1024u));
    }
    canvas.drawString(line, 7, 23);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    if (context.diagnostics == nullptr || context.diagnostics->size() == 0) {
        setUiFont(canvas, language);
        canvas.drawString(localized(language, "No diagnostic events", "没有诊断事件"),
                          7, 43);
    } else {
        setTechnicalFont(canvas);
        const std::size_t shown = context.diagnostics->size() < 5 ? context.diagnostics->size() : 5;
        for (std::size_t index = 0; index < shown; ++index) {
            canvas.drawString(context.diagnostics->newest(index), 7,
                              42 + static_cast<int16_t>(index) * 14);
        }
    }
    drawHint(canvas, language, "DEL BACK", "DEL 返回");
}

void drawConfirmation(M5Canvas& canvas, SettingsPage page, const char* wifiSsid,
                      UiLanguage language) {
    setUiFont(canvas, language);
    const char* title = localized(language, "FORGET PAIRED HOST?", "忘记已配对主机？");
    const char* detail = localized(language, "Disconnect and pair a new Mac",
                                   "断开后可配对新 Mac");
    if (page == SettingsPage::ConfirmForgetWifi) {
        title = localized(language, "FORGET WI-FI NETWORK?", "忘记这个 Wi-Fi？");
        detail = wifiSsid != nullptr && wifiSsid[0] != '\0' ? wifiSsid
                    : localized(language, "Erase saved password", "删除已保存密码");
    } else if (page == SettingsPage::ConfirmRestart) {
        title = localized(language, "RESTART POCKET DECK?", "重新启动 Pocket Deck？");
        detail = localized(language, "Current settings are preserved", "当前设置会保留");
    } else if (page == SettingsPage::ConfirmFormatStorage) {
        title = localized(language, "FORMAT TF CARD?", "格式化 TF 卡？");
        detail = localized(language, "ERASE ALL DATA ON THE CARD", "这会清除卡内全部数据");
    } else if (page == SettingsPage::ConfirmFactoryReset) {
        title = localized(language, "FACTORY RESET?", "恢复出厂设置？");
        detail = localized(language, "Erase settings, Wi-Fi and BLE",
                           "清除设置、Wi-Fi 和蓝牙");
    }
    canvas.fillRoundRect(17, 31, 206, 72, 7, theme::kPanelRaised);
    canvas.drawRoundRect(17, 31, 206, 72, 7, theme::kWarning);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kWarning, theme::kPanelRaised);
    canvas.drawString(title, 120, 51);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(detail, 120, 70);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(localized(language, "ENTER CONFIRM     DEL CANCEL",
                                "ENTER 确认     DEL 取消"),
                      120, 89);
}

}  // namespace

void SettingsApp::onEnter(SystemContext&) {
    model_.reset();
    clearWifiEntry();
}

void SettingsApp::onExit(SystemContext&) {
    clearWifiEntry();
}

void SettingsApp::update(uint32_t, SystemContext&) {}

void SettingsApp::onInput(const InputEvent& event, SystemContext& context) {
    if (model_.page() == SettingsPage::WifiPassword) {
        if (event.character != '\0') {
            const std::size_t length = std::strlen(wifiPassword_.data());
            if (length + 1 < wifiPassword_.size()) {
                wifiPassword_[length] = event.character;
                wifiPassword_[length + 1] = '\0';
            }
        } else if (event.action == InputAction::Erase) {
            const std::size_t length = std::strlen(wifiPassword_.data());
            if (length > 0) wifiPassword_[length - 1] = '\0';
        } else if (event.action == InputAction::Back) {
            wifiPassword_.fill('\0');
            model_.cancelWifiPassword();
        } else if (event.action == InputAction::Confirm) {
            context.requestWifiConnect(selectedSsid_.data(), wifiPassword_.data());
            wifiPassword_.fill('\0');
            model_.finishWifiConnection();
        }
        return;
    }

    const WifiSnapshot wifi = context.wifi != nullptr ? context.wifi->snapshot()
                                                       : WifiSnapshot{};
    const SettingsResult result =
        model_.handle(event.action, wifi.networkCount, wifi.savedNetworkCount);
    switch (result.effect) {
        case SettingsEffect::None: break;
        case SettingsEffect::GoHome: context.requestApp(AppId::Launcher); break;
        case SettingsEffect::ToggleWifi: context.requestCommand(SystemCommand::ToggleWifi); break;
        case SettingsEffect::StartWifiScan:
            context.requestCommand(SystemCommand::StartWifiScan);
            break;
        case SettingsEffect::SelectWifiNetwork: {
            if (wifi.networkCount == 0) break;
            const uint8_t index = std::min<uint8_t>(model_.selectedRow(), wifi.networkCount - 1);
            selectedSsid_.fill('\0');
            std::strncpy(selectedSsid_.data(), wifi.networks[index].ssid.data(),
                         selectedSsid_.size() - 1);
            wifiPassword_.fill('\0');
            if (wifi.networks[index].saved) {
                context.requestWifiConnect(selectedSsid_.data(), "");
                model_.finishWifiConnection();
            } else if (wifi.networks[index].secured) {
                model_.openWifiPassword();
            } else {
                context.requestWifiConnect(selectedSsid_.data(), "");
                model_.finishWifiConnection();
            }
            break;
        }
        case SettingsEffect::SelectWifiForForget: {
            if (wifi.savedNetworkCount == 0) break;
            const uint8_t index =
                std::min<uint8_t>(model_.selectedRow(), wifi.savedNetworkCount - 1);
            selectedSsid_.fill('\0');
            std::strncpy(selectedSsid_.data(), wifi.savedNetworks[index].ssid.data(),
                         selectedSsid_.size() - 1);
            break;
        }
        case SettingsEffect::ForgetWifi:
            context.requestWifiForget(selectedSsid_.data());
            selectedSsid_.fill('\0');
            break;
        case SettingsEffect::ToggleBluetooth:
            context.requestCommand(SystemCommand::ToggleBluetooth);
            break;
        case SettingsEffect::DisconnectBluetooth:
            context.requestCommand(SystemCommand::DisconnectBluetooth);
            break;
        case SettingsEffect::ForgetHost: context.requestCommand(SystemCommand::ForgetHost); break;
        case SettingsEffect::ToggleLanguage:
            context.requestCommand(SystemCommand::ToggleLanguage);
            break;
        case SettingsEffect::MountStorage:
            context.requestCommand(SystemCommand::MountStorage);
            break;
        case SettingsEffect::FormatStorage:
            context.requestCommand(SystemCommand::FormatStorage);
            break;
        case SettingsEffect::Restart: context.requestCommand(SystemCommand::Restart); break;
        case SettingsEffect::FactoryReset:
            context.requestCommand(SystemCommand::FactoryReset);
            break;
    }
}

void SettingsApp::render(Display& display, const SystemContext& context) {
    const BleKeyboardSnapshot ble = context.bleKeyboard != nullptr
                                        ? context.bleKeyboard->snapshot()
                                        : BleKeyboardSnapshot{};
    const WifiSnapshot wifi = context.wifi != nullptr ? context.wifi->snapshot()
                                                       : WifiSnapshot{};
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display, makeStatusBarData(
                               localized(language, "SETTINGS", "设置"), context));
    auto& canvas = display.canvas();
    switch (model_.page()) {
        case SettingsPage::Categories:
            drawCategories(canvas, model_, context, wifi, ble, language);
            break;
        case SettingsPage::Wifi: drawWifi(canvas, model_, wifi, language); break;
        case SettingsPage::WifiNetworks:
            drawWifiNetworks(canvas, model_, wifi, language);
            break;
        case SettingsPage::WifiSavedNetworks:
            drawWifiSavedNetworks(canvas, model_, wifi, language);
            break;
        case SettingsPage::WifiPassword:
            drawWifiPassword(canvas, selectedSsid_.data(), wifiPassword_.data(), language);
            break;
        case SettingsPage::WifiDiagnostics:
            drawWifiDiagnostics(canvas, wifi, language);
            break;
        case SettingsPage::Bluetooth:
            drawBluetooth(canvas, model_, context, ble, language);
            break;
        case SettingsPage::System: drawSystem(canvas, model_, context, language); break;
        case SettingsPage::Diagnostics: drawDiagnostics(canvas, context, language); break;
        case SettingsPage::Storage: drawStorage(canvas, model_, context, language); break;
        case SettingsPage::ConfirmForgetWifi:
        case SettingsPage::ConfirmForgetHost:
        case SettingsPage::ConfirmRestart:
        case SettingsPage::ConfirmFormatStorage:
        case SettingsPage::ConfirmFactoryReset:
            drawConfirmation(canvas, model_.page(), selectedSsid_.data(), language);
            break;
    }
}

void SettingsApp::clearWifiEntry() {
    selectedSsid_.fill('\0');
    wifiPassword_.fill('\0');
}

}  // namespace pd
