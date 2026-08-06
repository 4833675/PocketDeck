#include "ui/status_bar.h"

#include <cstdio>

#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "ui/localized_font.h"
#include "ui/theme.h"

namespace pd {
namespace {

uint16_t connectionColor(bool enabled, bool active, bool connected) {
    if (!enabled) return theme::kError;
    if (!active) return theme::kMuted;
    return connected ? theme::kPrimary : theme::kWarning;
}

}  // namespace

StatusBarData makeStatusBarData(const char* title, const SystemContext& context) {
    StatusBarData data;
    data.title = title;
    data.bleEnabled = context.bleEnabled;
    data.bleActive = context.bleActive;
    data.bleConnected = context.bleConnected;
    data.wifiEnabled = context.wifiEnabled;
    data.wifiActive = context.wifiActive;
    data.wifiConnected = context.wifiConnected;
    data.clockValid = context.clockValid;
    data.clockHour = context.clockHour;
    data.clockMinute = context.clockMinute;
    data.batteryPercent = context.batteryPercent;
    return data;
}

void drawStatusBar(Display& display, const StatusBarData& data) {
    auto& canvas = display.canvas();
    canvas.fillRect(0, 0, config::kScreenWidth, theme::kStatusHeight, theme::kPanel);
    setFontForText(canvas, data.title);
    canvas.setTextColor(theme::kPrimary, theme::kPanel);
    canvas.setTextDatum(middle_left);
    canvas.drawString(data.title, 6, theme::kStatusHeight / 2);

    setTechnicalFont(canvas);

    char battery[8];
    std::snprintf(battery, sizeof(battery), "%u%%",
                  static_cast<unsigned>(data.batteryPercent));
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_right);
    constexpr int16_t rightEdge = config::kScreenWidth - 6;
    canvas.drawString(battery, rightEdge, theme::kStatusHeight / 2);

    int16_t indicatorRight = rightEdge - canvas.textWidth(battery) - 7;
    canvas.setTextColor(connectionColor(data.bleEnabled, data.bleActive,
                                        data.bleConnected),
                        theme::kPanel);
    canvas.drawString("BT", indicatorRight, theme::kStatusHeight / 2);
    indicatorRight -= canvas.textWidth("BT") + 7;
    canvas.setTextColor(connectionColor(data.wifiEnabled, data.wifiActive,
                                        data.wifiConnected),
                        theme::kPanel);
    canvas.drawString("WiFi", indicatorRight, theme::kStatusHeight / 2);

    char clock[6] = "--:--";
    if (data.clockValid) {
        std::snprintf(clock, sizeof(clock), "%02u:%02u",
                      static_cast<unsigned>(data.clockHour),
                      static_cast<unsigned>(data.clockMinute));
    }
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(data.clockValid ? theme::kText : theme::kMuted, theme::kPanel);
    canvas.drawString(clock, config::kScreenWidth / 2, theme::kStatusHeight / 2);
}

}  // namespace pd
