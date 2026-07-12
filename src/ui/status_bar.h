#pragma once

#include <cstdint>

namespace pd {

class Display;

struct StatusBarData {
    const char* title = "POCKET DECK";
    bool bleEnabled = true;
    bool bleConnected = false;
    uint8_t batteryPercent = 0;
};

void drawStatusBar(Display& display, const StatusBarData& data);

}  // namespace pd

