#include "apps/settings/settings_app.h"

#include <cstdio>

#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/ble_keyboard_service.h"
#include "services/diagnostics_service.h"
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

void drawCategories(M5Canvas& canvas, const SettingsModel& model,
                    const SystemContext& context, const BleKeyboardSnapshot& ble) {
    canvas.fillRoundRect(5, 23, 77, 88, 5, theme::kPanel);
    const bool bluetooth = model.category() == SettingsCategory::Bluetooth;
    for (uint8_t index = 0; index < 2; ++index) {
        const bool selected = (index == 0) == bluetooth;
        const int16_t y = 31 + static_cast<int16_t>(index) * 34;
        canvas.fillRoundRect(10, y, 67, 25, 4,
                             selected ? theme::kPanelRaised : theme::kPanel);
        if (selected) canvas.drawRoundRect(10, y, 67, 25, 4, theme::kPrimary);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(selected ? theme::kText : theme::kMuted,
                            selected ? theme::kPanelRaised : theme::kPanel);
        canvas.drawString(index == 0 ? "BLUETOOTH" : "SYSTEM", 15, y + 12);
    }

    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    canvas.drawString(bluetooth ? "BLUETOOTH" : "SYSTEM", 91, 27);
    canvas.setTextColor(theme::kText, theme::kBackground);
    char line[48];
    if (bluetooth) {
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
    if (page == SettingsPage::ConfirmRestart) {
        title = "RESTART POCKET DECK?";
        detail = "Current settings are preserved";
    } else if (page == SettingsPage::ConfirmFactoryReset) {
        title = "FACTORY RESET?";
        detail = "Erase settings and BLE bond";
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
}

void SettingsApp::onExit(SystemContext&) {}
void SettingsApp::update(uint32_t, SystemContext&) {}

void SettingsApp::onInput(const InputEvent& event, SystemContext& context) {
    const SettingsResult result = model_.handle(event.action);
    switch (result.effect) {
        case SettingsEffect::None: break;
        case SettingsEffect::GoHome: context.requestApp(AppId::Launcher); break;
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
    drawStatusBar(display, {"SETTINGS", ble.enabled, ble.connected, context.batteryPercent});
    auto& canvas = display.canvas();
    switch (model_.page()) {
        case SettingsPage::Categories: drawCategories(canvas, model_, context, ble); break;
        case SettingsPage::Bluetooth: drawBluetooth(canvas, model_, context, ble); break;
        case SettingsPage::System: drawSystem(canvas, model_, context); break;
        case SettingsPage::Diagnostics: drawDiagnostics(canvas, context); break;
        case SettingsPage::ConfirmForgetHost:
        case SettingsPage::ConfirmRestart:
        case SettingsPage::ConfirmFactoryReset: drawConfirmation(canvas, model_.page()); break;
    }
}

}  // namespace pd
