#include "apps/keyboard/keyboard_app.h"

#include <cstdio>

#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/ble_keyboard_service.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* stateTitle(BleKeyboardState state) {
    switch (state) {
        case BleKeyboardState::Disabled: return "BLUETOOTH OFF";
        case BleKeyboardState::Advertising: return "WAITING FOR MAC";
        case BleKeyboardState::Pairing: return "PAIRING";
        case BleKeyboardState::Connected: return "CONNECTED";
        case BleKeyboardState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

void drawModifier(M5Canvas& canvas, int16_t x, const char* label, bool active) {
    const uint16_t background = active ? theme::kPrimary : theme::kPanel;
    const uint16_t foreground = active ? theme::kBackground : theme::kMuted;
    canvas.fillRoundRect(x, 91, 46, 14, 3, background);
    canvas.drawRoundRect(x, 91, 46, 14, 3, active ? theme::kPrimary : theme::kBorder);
    canvas.setTextColor(foreground, background);
    canvas.setTextDatum(middle_center);
    canvas.drawString(label, x + 23, 98);
}

}  // namespace

void KeyboardApp::onEnter(SystemContext&) {}

void KeyboardApp::onExit(SystemContext& context) {
    if (context.bleKeyboard != nullptr) context.bleKeyboard->sendReport(HidReport{});
    context.activeModifiers = 0;
}

void KeyboardApp::onInput(const InputEvent&, SystemContext&) {}
void KeyboardApp::update(uint32_t, SystemContext&) {}

void KeyboardApp::render(Display& display, const SystemContext& context) {
    const BleKeyboardSnapshot snapshot = context.bleKeyboard != nullptr
                                             ? context.bleKeyboard->snapshot()
                                             : BleKeyboardSnapshot{};
    auto& canvas = display.canvas();
    drawStatusBar(display, {"KEYBOARD", snapshot.enabled,
                            snapshot.state == BleKeyboardState::Connected,
                            context.batteryPercent});

    const uint16_t stateColor = snapshot.state == BleKeyboardState::Connected
                                    ? theme::kPrimary
                                    : (snapshot.state == BleKeyboardState::Error ? theme::kError
                                                                                 : theme::kWarning);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(stateColor, theme::kBackground);
    canvas.setTextSize(2);
    canvas.drawString(stateTitle(snapshot.state), config::kScreenWidth / 2, 38);
    canvas.setTextSize(1);

    if (snapshot.state == BleKeyboardState::Pairing && !snapshot.bonded) {
        char passkey[8];
        std::snprintf(passkey, sizeof(passkey), "%06lu",
                      static_cast<unsigned long>(snapshot.passkey));
        canvas.setTextColor(theme::kText, theme::kBackground);
        canvas.setTextSize(2);
        canvas.drawString(passkey, config::kScreenWidth / 2, 63);
        canvas.setTextSize(1);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("Enter this code on your Mac", config::kScreenWidth / 2, 79);
    } else if (snapshot.state == BleKeyboardState::Connected) {
        canvas.setTextColor(theme::kText, theme::kBackground);
        canvas.drawString("Mac  /  encrypted BLE HID", config::kScreenWidth / 2, 65);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString("Typing is active", config::kScreenWidth / 2, 79);
    } else if (snapshot.state == BleKeyboardState::Error) {
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        const char* error = context.bleKeyboard != nullptr ? context.bleKeyboard->errorText()
                                                           : "service unavailable";
        canvas.drawString(error, config::kScreenWidth / 2, 67);
    } else {
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(snapshot.bonded ? "Advertising to paired Mac"
                                          : "Open Bluetooth Settings on Mac",
                          config::kScreenWidth / 2, 67);
    }

    drawModifier(canvas, 12, "CTRL", (context.activeModifiers & 0x01u) != 0);
    drawModifier(canvas, 70, "SHIFT", (context.activeModifiers & 0x02u) != 0);
    drawModifier(canvas, 128, "OPT", (context.activeModifiers & 0x04u) != 0);
    drawModifier(canvas, 186, "CMD", (context.activeModifiers & 0x08u) != 0);

    const int16_t hintY = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, hintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.drawString("G0 HOME   HOLD G0 QUICK SETTINGS", config::kScreenWidth / 2,
                      hintY + theme::kHintHeight / 2);
}

}  // namespace pd

