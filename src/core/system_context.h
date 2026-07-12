#pragma once

#include <cstdint>

#include "core/app_id.h"

namespace pd {

struct SystemContext {
    AppId requestedApp = AppId::None;
    uint8_t batteryPercent = 0;
    bool bleEnabled = true;
    bool bleConnected = false;

    void requestApp(AppId app) { requestedApp = app; }
    AppId takeRequestedApp() {
        const AppId requested = requestedApp;
        requestedApp = AppId::None;
        return requested;
    }
};

}  // namespace pd

