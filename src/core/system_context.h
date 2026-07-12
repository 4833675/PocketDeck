#pragma once

#include <cstdint>

#include "core/app_id.h"

namespace pd {

class BleKeyboardService;

struct SystemContext {
    AppId requestedApp = AppId::None;
    uint8_t batteryPercent = 0;
    bool bleEnabled = true;
    bool bleConnected = false;
    uint8_t activeModifiers = 0;
    BleKeyboardService* bleKeyboard = nullptr;

    void requestApp(AppId app) { requestedApp = app; }
    AppId takeRequestedApp() {
        const AppId requested = requestedApp;
        requestedApp = AppId::None;
        return requested;
    }
};

}  // namespace pd
