#pragma once

#include <cstdint>

namespace pd {

class Display;
struct SystemContext;

struct StatusBarData {
    const char* title = "POCKET DECK";
    bool bleEnabled = true;
    bool bleActive = false;
    bool bleConnected = false;
    bool wifiEnabled = false;
    bool wifiActive = false;
    bool wifiConnected = false;
    bool clockValid = false;
    uint8_t clockHour = 0;
    uint8_t clockMinute = 0;
    uint8_t batteryPercent = 0;
};

StatusBarData makeStatusBarData(const char* title, const SystemContext& context);
void drawStatusBar(Display& display, const StatusBarData& data);

}  // namespace pd
