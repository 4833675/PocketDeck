#include "ui/quick_settings.h"

#include <cstdio>

#include "drivers/display.h"
#include "ui/theme.h"

namespace pd {
namespace {

void drawMeter(M5Canvas& canvas, int16_t y, const char* label, uint8_t value) {
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(label, 29, y);
    canvas.drawRoundRect(94, y - 4, 82, 8, 3, theme::kBorder);
    const int16_t width = static_cast<int16_t>((78u * value) / 100u);
    if (width > 0) canvas.fillRoundRect(96, y - 2, width, 4, 2, theme::kPrimary);
    char valueText[6];
    std::snprintf(valueText, sizeof(valueText), "%u", static_cast<unsigned>(value));
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(valueText, 207, y);
}

}  // namespace

void QuickSettings::render(Display& display, uint8_t batteryPercent) const {
    if (!model_.active()) return;
    auto& canvas = display.canvas();
    canvas.fillRoundRect(13, 18, 214, 101, 8, theme::kPanelRaised);
    canvas.drawRoundRect(13, 18, 214, 101, 8, theme::kPrimary);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kPrimary, theme::kPanelRaised);
    canvas.drawString("QUICK SETTINGS", 25, 31);
    char battery[12];
    std::snprintf(battery, sizeof(battery), "BAT %u%%", static_cast<unsigned>(batteryPercent));
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(battery, 215, 31);

    drawMeter(canvas, 52, "BRIGHT", model_.values().brightness);
    drawMeter(canvas, 70, "VOLUME", model_.values().volume);

    canvas.setTextDatum(middle_left);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString("BLUETOOTH", 29, 89);
    const bool enabled = model_.values().bleEnabled;
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(enabled ? theme::kPrimary : theme::kWarning, theme::kPanelRaised);
    canvas.drawString(enabled ? "ON" : "OFF", 207, 89);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString("LR BRIGHT  UD VOL  ENTER BT  DEL CLOSE", 120, 107);
}

}  // namespace pd
