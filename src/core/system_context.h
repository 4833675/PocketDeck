#pragma once

#include <cstdint>

#include "core/app_id.h"

namespace pd {

class BleKeyboardService;
class DiagnosticsService;
class GpsService;
struct SystemSettings;

enum class SystemCommand : uint8_t {
    None,
    ToggleBluetooth,
    DisconnectBluetooth,
    ForgetHost,
    Restart,
    FactoryReset,
};

struct SystemContext {
    AppId requestedApp = AppId::None;
    uint8_t batteryPercent = 0;
    bool bleEnabled = true;
    bool bleConnected = false;
    uint8_t activeModifiers = 0;
    uint32_t uptimeMs = 0;
    uint32_t freeHeap = 0;
    uint32_t minimumFreeHeap = 0;
    const char* resetReason = "unknown";
    BleKeyboardService* bleKeyboard = nullptr;
    GpsService* gps = nullptr;
    const DiagnosticsService* diagnostics = nullptr;
    const SystemSettings* settings = nullptr;

    void requestApp(AppId app) { requestedApp = app; }
    AppId takeRequestedApp() {
        const AppId requested = requestedApp;
        requestedApp = AppId::None;
        return requested;
    }

    void requestCommand(SystemCommand command) { requestedCommand_ = command; }
    SystemCommand takeRequestedCommand() {
        const SystemCommand requested = requestedCommand_;
        requestedCommand_ = SystemCommand::None;
        return requested;
    }

private:
    SystemCommand requestedCommand_ = SystemCommand::None;
};

}  // namespace pd
