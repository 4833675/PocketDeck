#include "ui/quick_settings.h"

#include <cstdio>

#include "core/localization.h"
#include "drivers/display.h"
#include "ui/localized_font.h"
#include "ui/theme.h"

namespace pd {
namespace {

void drawMeter(M5Canvas& canvas, int16_t y, const char* label, uint8_t value) {
    setFontForText(canvas, label);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(label, 29, y);
    canvas.drawRoundRect(94, y - 4, 82, 8, 3, theme::kBorder);
    const int16_t width = static_cast<int16_t>((78u * value) / 100u);
    if (width > 0) canvas.fillRoundRect(96, y - 2, width, 4, 2, theme::kPrimary);
    char valueText[6];
    std::snprintf(valueText, sizeof(valueText), "%u", static_cast<unsigned>(value));
    setTechnicalFont(canvas);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(valueText, 207, y);
}

}  // namespace

void QuickSettings::render(Display& display, uint8_t batteryPercent,
                           UiLanguage language) const {
    if (!model_.active()) return;
    auto& canvas = display.canvas();
    canvas.fillRoundRect(13, 18, 214, 101, 8, theme::kPanelRaised);
    canvas.drawRoundRect(13, 18, 214, 101, 8, theme::kPrimary);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kPrimary, theme::kPanelRaised);
    canvas.drawString(localized(language, "QUICK SETTINGS", "快捷设置"), 25, 31);
    char battery[24];
    std::snprintf(battery, sizeof(battery),
                  localized(language, "BAT %u%%", "电量 %u%%"),
                  static_cast<unsigned>(batteryPercent));
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(battery, 215, 31);

    drawMeter(canvas, 52, localized(language, "BRIGHT", "亮度"),
              model_.values().brightness);
    drawMeter(canvas, 70, localized(language, "VOLUME", "音量"),
              model_.values().volume);

    setUiFont(canvas, language);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(localized(language, "BLUETOOTH", "蓝牙"), 29, 89);
    const bool enabled = model_.values().bleEnabled;
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(enabled ? theme::kPrimary : theme::kWarning, theme::kPanelRaised);
    canvas.drawString(enabled ? localized(language, "ON", "开")
                              : localized(language, "OFF", "关"),
                      207, 89);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(localized(language,
                                "LR BRIGHT  UD VOL  ENTER BT  DEL CLOSE",
                                "LR 亮度  UD 音量  ENTER 蓝牙  DEL 关闭"),
                      120, 107);
}

}  // namespace pd
