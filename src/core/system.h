#pragma once

#include "apps/launcher/launcher_app.h"
#include "core/g0_gesture.h"
#include "core/input_router.h"
#include "core/system_context.h"
#include "drivers/board.h"
#include "drivers/display.h"
#include "services/ble_keyboard_service.h"

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
    void render();

    Board board_;
    Display display_;
    BleKeyboardService bleKeyboard_;
    InputRouter inputRouter_;
    G0Gesture g0Gesture_;
    SystemContext context_;
    LauncherApp launcher_;
    App* current_ = nullptr;
    uint32_t lastRenderMs_ = 0;
};

}  // namespace pd
