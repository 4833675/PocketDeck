#include "apps/settings/settings_app.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "core/system_context.h"
#include "core/system_settings.h"
#include "core/wifi_data.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/ble_keyboard_service.h"
#include "services/diagnostics_service.h"
#include "services/wifi_service.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* bleStateName(BleKeyboardState state) {
    switch (state) {
        case BleKeyboardState::Disabled: return "OFF";
        case BleKeyboardState::Advertising: return "ADVERTISING";
        case BleKeyboardState::Pairing: return "PAIRING";
        case BleKeyboardState::Connected: return "CONNECTED";
        case BleKeyboardState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

uint16_t wifiStateColor(const WifiSnapshot& wifi) {
    if (!wifi.enabled || wifi.state == WifiState::Disabled) return theme::kError;
    if (wifi.connected) return theme::kPrimary;
    if (wifi.state == WifiState::Error) return theme::kError;
    return theme::kWarning;
}

void drawHint(M5Canvas& canvas, const char* text) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(text, config::kScreenWidth / 2, y + theme::kHintHeight / 2);
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
                    const BleKeyboardSnapshot& ble) {
    canvas.fillRoundRect(5, 23, 77, 88, 5, theme::kPanel);
    constexpr const char* labels[] = {"WI-FI", "BLUETOOTH", "SYSTEM"};
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
        canvas.drawString("WI-FI", 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "State  %s", wifiStateLabel(wifi.state));
        canvas.drawString(line, 91, 45);
        char ssid[24];
        compactSsid(ssid, sizeof(ssid), wifi.ssid.data(), 18);
        canvas.drawString(ssid, 91, 59);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        if (wifi.connected) {
            std::snprintf(line, sizeof(line), "%s  %ld dBm", wifi.ip.data(),
                          static_cast<long>(wifi.rssi));
            canvas.drawString(line, 91, 76);
            canvas.drawString(wifi.ntpSynced ? "NTP synchronized" : "Waiting for NTP", 91, 92);
        } else {
            canvas.drawString(wifi.hasSavedNetwork ? "Saved network available"
                                                   : "No saved network",
                              91, 76);
            canvas.drawString("Scan to connect", 91, 92);
        }
    } else if (model.category() == SettingsCategory::Bluetooth) {
        canvas.setTextColor(theme::kPrimary, theme::kBackground);
        canvas.drawString("BLUETOOTH", 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "State  %s", bleStateName(ble.state));
        canvas.drawString(line, 91, 45);
        canvas.drawString(context.settings != nullptr ? context.settings->deviceName.data()
                                                       : "Pocket Deck",
                          91, 59);
        std::snprintf(line, sizeof(line), "Host   %s",
                      context.settings != nullptr ? context.settings->hostLabel.data() : "Mac");
        canvas.drawString(line, 91, 73);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(ble.bonded ? "One host paired" : "No paired host", 91, 90);
    } else {
        canvas.setTextColor(theme::kPrimary, theme::kBackground);
        canvas.drawString("SYSTEM", 91, 27);
        canvas.setTextColor(theme::kText, theme::kBackground);
        std::snprintf(line, sizeof(line), "Version %s", config::kFirmwareVersion);
        canvas.drawString(line, 91, 45);
        std::snprintf(line, sizeof(line), "Reset   %s", context.resetReason);
        canvas.drawString(line, 91, 59);
        std::snprintf(line, sizeof(line), "Heap    %lu KB",
                      static_cast<unsigned long>(context.freeHeap / 1024u));
        canvas.drawString(line, 91, 73);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("Diagnostics & reset", 91, 90);
    }
    drawHint(canvas, "FN+;/. MOVE   ENTER OPEN   DEL HOME");
}

void drawWifi(M5Canvas& canvas, const SettingsModel& model, const WifiSnapshot& wifi) {
    drawChoice(canvas, 22, wifi.enabled ? "Wi-Fi  ON" : "Wi-Fi  OFF",
               model.selectedRow() == 0);
    drawChoice(canvas, 44, "Scan networks", model.selectedRow() == 1);
    drawChoice(canvas, 66, "Network info", model.selectedRow() == 2);
    drawChoice(canvas, 88, "Forget network", model.selectedRow() == 3);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(wifiStateColor(wifi), theme::kBackground);
    canvas.drawString(wifiStateLabel(wifi.state), 121, 23);
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
        canvas.drawString(wifi.hasSavedNetwork ? "Saved" : "No credentials", 121, 58);
        canvas.drawString("Not connected", 121, 74);
    }
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(wifi.ntpSynced ? "NTP ready" : "NTP --", 121, 92);
    drawHint(canvas, "FN+;/. MOVE   ENTER SELECT   DEL BACK");
}

void drawWifiNetworks(M5Canvas& canvas, const SettingsModel& model,
                      const WifiSnapshot& wifi) {
    canvas.setTextDatum(top_left);
    if (!wifi.enabled) {
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString("WI-FI IS OFF", 8, 27);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("Enable it on the previous page", 8, 47);
    } else if (wifi.state == WifiState::Scanning && wifi.networkCount == 0) {
        canvas.setTextColor(theme::kWarning, theme::kBackground);
        canvas.drawString("SCANNING NETWORKS...", 8, 27);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("BLE remains available", 8, 47);
    } else if (wifi.networkCount == 0) {
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("No networks found", 8, 27);
        canvas.drawString("Press TAB to scan again", 8, 47);
    } else {
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
            std::snprintf(line, sizeof(line), "%c %-24.24s %4ld",
                          wifi.networks[index].secured ? '*' : 'o',
                          wifi.networks[index].ssid.data(),
                          static_cast<long>(wifi.networks[index].rssi));
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(active ? theme::kText : theme::kMuted, background);
            canvas.drawString(line, 11, y + 8);
        }
    }
    drawHint(canvas, "FN+;/. MOVE  ENTER CONNECT  TAB SCAN  DEL BACK");
}

void drawWifiPassword(M5Canvas& canvas, const char* ssid, const char* password) {
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    canvas.drawString("WI-FI PASSWORD", 8, 25);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char network[42];
    std::snprintf(network, sizeof(network), "Network: %.27s", ssid);
    canvas.drawString(network, 8, 43);

    const std::size_t length = std::strlen(password);
    char mask[33];
    const std::size_t shown = std::min<std::size_t>(length, sizeof(mask) - 1);
    std::memset(mask, '*', shown);
    mask[shown] = '\0';
    canvas.fillRoundRect(7, 61, 226, 24, 4, theme::kPanelRaised);
    canvas.drawRoundRect(7, 61, 226, 24, 4, theme::kPrimary);
    canvas.setTextDatum(middle_left);
    canvas.drawString(mask, 13, 73);

    char count[24];
    std::snprintf(count, sizeof(count), "%u characters", static_cast<unsigned>(length));
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(count, 8, 91);
    canvas.drawString("Stored by ESP32 Wi-Fi only", 105, 91);
    drawHint(canvas, "TYPE  ENTER CONNECT  DEL ERASE  FN+` CANCEL");
}

void drawWifiDiagnostics(M5Canvas& canvas, const WifiSnapshot& wifi) {
    canvas.setTextDatum(top_left);
    char line[64];
    canvas.setTextColor(wifiStateColor(wifi), theme::kBackground);
    std::snprintf(line, sizeof(line), "STATE %-11s STATUS %d", wifiStateLabel(wifi.state),
                  static_cast<int>(wifi.lastStatus));
    canvas.drawString(line, 7, 23);
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
    drawHint(canvas, "DEL BACK");
}

void drawBluetooth(M5Canvas& canvas, const SettingsModel& model,
                   const SystemContext& context, const BleKeyboardSnapshot& ble) {
    drawChoice(canvas, 27, ble.enabled ? "Bluetooth  ON" : "Bluetooth  OFF",
               model.selectedRow() == 0);
    drawChoice(canvas, 50, "Disconnect", model.selectedRow() == 1);
    drawChoice(canvas, 73, "Forget host", model.selectedRow() == 2);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    canvas.drawString(bleStateName(ble.state), 121, 28);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString(context.settings != nullptr ? context.settings->deviceName.data()
                                                   : "Pocket Deck",
                      121, 47);
    char host[34];
    std::snprintf(host, sizeof(host), "Host: %s",
                  context.settings != nullptr ? context.settings->hostLabel.data() : "Mac");
    canvas.drawString(host, 121, 62);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(ble.encrypted ? "Encrypted" : "Not encrypted", 121, 79);
    canvas.drawString(ble.bonded ? "Bond stored" : "Ready to pair", 121, 94);
    drawHint(canvas, "FN+;/. MOVE   ENTER SELECT   DEL BACK");
}

void drawSystem(M5Canvas& canvas, const SettingsModel& model, const SystemContext& context) {
    drawChoice(canvas, 27, "Diagnostics", model.selectedRow() == 0);
    drawChoice(canvas, 50, "Restart", model.selectedRow() == 1);
    drawChoice(canvas, 73, "Factory reset", model.selectedRow() == 2);

    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[48];
    std::snprintf(line, sizeof(line), "v%s", config::kFirmwareVersion);
    canvas.drawString(line, 121, 29);
    std::snprintf(line, sizeof(line), "Up %lus",
                  static_cast<unsigned long>(context.uptimeMs / 1000u));
    canvas.drawString(line, 121, 45);
    std::snprintf(line, sizeof(line), "Heap %lu KB",
                  static_cast<unsigned long>(context.freeHeap / 1024u));
    canvas.drawString(line, 121, 61);
    std::snprintf(line, sizeof(line), "Min  %lu KB",
                  static_cast<unsigned long>(context.minimumFreeHeap / 1024u));
    canvas.drawString(line, 121, 77);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(context.resetReason, 121, 94);
    drawHint(canvas, "FN+;/. MOVE   ENTER SELECT   DEL BACK");
}

void drawDiagnostics(M5Canvas& canvas, const SystemContext& context) {
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[48];
    std::snprintf(line, sizeof(line), "RESET %-10s  HEAP %luK", context.resetReason,
                  static_cast<unsigned long>(context.freeHeap / 1024u));
    canvas.drawString(line, 7, 23);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    if (context.diagnostics == nullptr || context.diagnostics->size() == 0) {
        canvas.drawString("No diagnostic events", 7, 43);
    } else {
        const std::size_t shown = context.diagnostics->size() < 5 ? context.diagnostics->size() : 5;
        for (std::size_t index = 0; index < shown; ++index) {
            canvas.drawString(context.diagnostics->newest(index), 7,
                              42 + static_cast<int16_t>(index) * 14);
        }
    }
    drawHint(canvas, "DEL BACK");
}

void drawConfirmation(M5Canvas& canvas, SettingsPage page) {
    const char* title = "FORGET PAIRED HOST?";
    const char* detail = "Disconnect and pair a new Mac";
    if (page == SettingsPage::ConfirmForgetWifi) {
        title = "FORGET WI-FI NETWORK?";
        detail = "Erase the saved password";
    } else if (page == SettingsPage::ConfirmRestart) {
        title = "RESTART POCKET DECK?";
        detail = "Current settings are preserved";
    } else if (page == SettingsPage::ConfirmFactoryReset) {
        title = "FACTORY RESET?";
        detail = "Erase settings, Wi-Fi and BLE";
    }
    canvas.fillRoundRect(17, 31, 206, 72, 7, theme::kPanelRaised);
    canvas.drawRoundRect(17, 31, 206, 72, 7, theme::kWarning);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kWarning, theme::kPanelRaised);
    canvas.drawString(title, 120, 51);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(detail, 120, 70);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString("ENTER CONFIRM     DEL CANCEL", 120, 89);
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
    const SettingsResult result = model_.handle(event.action, wifi.networkCount);
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
            if (wifi.networks[index].secured) {
                model_.openWifiPassword();
            } else {
                context.requestWifiConnect(selectedSsid_.data(), "");
                model_.finishWifiConnection();
            }
            break;
        }
        case SettingsEffect::ForgetWifi:
            context.requestCommand(SystemCommand::ForgetWifi);
            break;
        case SettingsEffect::ToggleBluetooth:
            context.requestCommand(SystemCommand::ToggleBluetooth);
            break;
        case SettingsEffect::DisconnectBluetooth:
            context.requestCommand(SystemCommand::DisconnectBluetooth);
            break;
        case SettingsEffect::ForgetHost: context.requestCommand(SystemCommand::ForgetHost); break;
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
    drawStatusBar(display, {"SETTINGS", ble.enabled, ble.connected, context.batteryPercent});
    auto& canvas = display.canvas();
    switch (model_.page()) {
        case SettingsPage::Categories: drawCategories(canvas, model_, context, wifi, ble); break;
        case SettingsPage::Wifi: drawWifi(canvas, model_, wifi); break;
        case SettingsPage::WifiNetworks: drawWifiNetworks(canvas, model_, wifi); break;
        case SettingsPage::WifiPassword:
            drawWifiPassword(canvas, selectedSsid_.data(), wifiPassword_.data());
            break;
        case SettingsPage::WifiDiagnostics: drawWifiDiagnostics(canvas, wifi); break;
        case SettingsPage::Bluetooth: drawBluetooth(canvas, model_, context, ble); break;
        case SettingsPage::System: drawSystem(canvas, model_, context); break;
        case SettingsPage::Diagnostics: drawDiagnostics(canvas, context); break;
        case SettingsPage::ConfirmForgetWifi:
        case SettingsPage::ConfirmForgetHost:
        case SettingsPage::ConfirmRestart:
        case SettingsPage::ConfirmFactoryReset: drawConfirmation(canvas, model_.page()); break;
    }
}

void SettingsApp::clearWifiEntry() {
    selectedSsid_.fill('\0');
    wifiPassword_.fill('\0');
}

}  // namespace pd
