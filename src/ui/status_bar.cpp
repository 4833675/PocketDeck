#include "ui/status_bar.h"

#include <cstdio>

#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "ui/theme.h"

namespace pd {

void drawStatusBar(Display& display, const StatusBarData& data) {
    auto& canvas = display.canvas();
    canvas.fillRect(0, 0, config::kScreenWidth, theme::kStatusHeight, theme::kPanel);
    canvas.setTextColor(theme::kPrimary, theme::kPanel);
    canvas.setTextDatum(middle_left);
    canvas.drawString(data.title, 6, theme::kStatusHeight / 2);

    char battery[8];
    std::snprintf(battery, sizeof(battery), "%u%%",
                  static_cast<unsigned>(data.batteryPercent));
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_right);
    constexpr int16_t rightEdge = config::kScreenWidth - 6;
    canvas.drawString(battery, rightEdge, theme::kStatusHeight / 2);

    const uint16_t btColor = !data.bleEnabled
                                 ? theme::kError
                                 : (data.bleConnected ? theme::kPrimary : theme::kWarning);
    canvas.setTextColor(btColor, theme::kPanel);
    canvas.drawString("BT", rightEdge - canvas.textWidth(battery) - 7,
                      theme::kStatusHeight / 2);
}

}  // namespace pd
