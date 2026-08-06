#include "apps/keyboard/keyboard_app.h"
#include "apps/keyboard/keyboard_app_text.h"

#include <cstdio>

#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/ble_keyboard_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

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
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    const BleKeyboardSnapshot snapshot = context.bleKeyboard != nullptr
                                             ? context.bleKeyboard->snapshot()
                                             : BleKeyboardSnapshot{};
    auto& canvas = display.canvas();
    drawStatusBar(display,
                  makeStatusBarData(localized(language, "KEYBOARD", "键盘"), context));

    const uint16_t stateColor = snapshot.state == BleKeyboardState::Connected
                                    ? theme::kPrimary
                                    : (snapshot.state == BleKeyboardState::Error ? theme::kError
                                                                                 : theme::kWarning);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(stateColor, theme::kBackground);
    setUiFont(canvas, language);
    canvas.setTextSize(isSimplifiedChinese(language) ? 1 : 2);
    canvas.drawString(localizedKeyboardStateLabel(snapshot.state, language),
                      config::kScreenWidth / 2, 38);
    setUiFont(canvas, language);

    if (snapshot.state == BleKeyboardState::Pairing && !snapshot.bonded) {
        char passkey[8];
        std::snprintf(passkey, sizeof(passkey), "%06lu",
                      static_cast<unsigned long>(snapshot.passkey));
        setTechnicalFont(canvas);
        canvas.setTextColor(theme::kText, theme::kBackground);
        canvas.setTextSize(2);
        canvas.drawString(passkey, config::kScreenWidth / 2, 63);
        canvas.setTextSize(1);
        setUiFont(canvas, language);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Enter this code on your Mac",
                                    "请在 Mac 上输入此配对码"),
                          config::kScreenWidth / 2, 79);
    } else if (snapshot.state == BleKeyboardState::Connected) {
        setUiFont(canvas, language);
        canvas.setTextColor(theme::kText, theme::kBackground);
        canvas.drawString(localized(language, "Mac  /  encrypted BLE HID",
                                    "Mac  /  加密 BLE HID"),
                          config::kScreenWidth / 2, 65);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Typing is active", "键盘输入已启用"),
                          config::kScreenWidth / 2, 79);
    } else if (snapshot.state == BleKeyboardState::Error) {
        setUiFont(canvas, language);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        const char* error = context.bleKeyboard != nullptr
                                ? localizedKeyboardErrorLabel(snapshot.error, language)
                                : localized(language, "service unavailable", "服务不可用");
        canvas.drawString(error, config::kScreenWidth / 2, 67);
    } else {
        setUiFont(canvas, language);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(snapshot.bonded
                              ? localized(language, "Advertising to paired Mac",
                                          "正在等待已配对的 Mac")
                              : localized(language, "Open Bluetooth Settings on Mac",
                                          "请打开 Mac 蓝牙设置"),
                          config::kScreenWidth / 2, 67);
    }

    setTechnicalFont(canvas);
    drawModifier(canvas, 12, "CTRL", (context.activeModifiers & 0x01u) != 0);
    drawModifier(canvas, 70, "SHIFT", (context.activeModifiers & 0x02u) != 0);
    drawModifier(canvas, 128, "OPT", (context.activeModifiers & 0x04u) != 0);
    drawModifier(canvas, 186, "CMD", (context.activeModifiers & 0x08u) != 0);

    const int16_t hintY = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, hintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.drawString(localized(language,
                                "G0 HOME   HOLD G0 QUICK SETTINGS",
                                "G0 主页   长按 G0 快捷设置"),
                      config::kScreenWidth / 2,
                      hintY + theme::kHintHeight / 2);
}

}  // namespace pd
