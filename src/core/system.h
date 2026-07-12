#pragma once

#include "apps/keyboard/keyboard_app.h"
#include "apps/launcher/launcher_app.h"
#include "apps/settings/settings_app.h"
#include "core/g0_gesture.h"
#include "core/input_router.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/board.h"
#include "drivers/display.h"
#include "services/ble_keyboard_service.h"
#include "services/diagnostics_service.h"
#include "services/settings_store.h"
#include "ui/quick_settings.h"

namespace pd {

class System {
public:
    System();
    void begin();
    void update();

private:
    App* appForId(AppId id);
    void openApp(AppId id);
    void goHome();
    void openQuickSettings();
    void closeQuickSettings();
    void handleQuickSettingsResult(const QuickSettingsResult& result);
    void handleSystemCommand(SystemCommand command);
    void refreshContext(uint32_t nowMs);
    void trackBleState(const BleKeyboardSnapshot& snapshot);
    bool saveSettings();
    void render();

    Board board_;
    Display display_;
    BleKeyboardService bleKeyboard_;
    DiagnosticsService diagnostics_;
    SettingsStore settingsStore_;
    SystemSettings settings_ = SystemSettings::defaults();
    InputRouter inputRouter_;
    G0Gesture g0Gesture_;
    SystemContext context_;
    LauncherApp launcher_;
    KeyboardApp keyboard_;
    SettingsApp settingsApp_;
    QuickSettings quickSettings_;
    App* current_ = nullptr;
    BleKeyboardSnapshot lastBleSnapshot_{};
    bool lastBleSnapshotValid_ = false;
    uint32_t lastRenderMs_ = 0;
};

}  // namespace pd
