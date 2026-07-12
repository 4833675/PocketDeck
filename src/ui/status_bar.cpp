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

    char status[32];
    const char* bt = !data.bleEnabled ? "BT OFF" : (data.bleConnected ? "BT ON" : "BT --");
    std::snprintf(status, sizeof(status), "%s  %u%%", bt,
                  static_cast<unsigned>(data.batteryPercent));
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_right);
    canvas.drawString(status, config::kScreenWidth - 6, theme::kStatusHeight / 2);
}

}  // namespace pd

