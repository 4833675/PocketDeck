#pragma once

#include <cstdint>

#include "ui/quick_settings_model.h"

namespace pd {

class Display;

class QuickSettings {
public:
    void open(QuickSettingsValues values) { model_.open(values); }
    QuickSettingsResult handle(InputAction action) { return model_.handle(action); }
    QuickSettingsResult close() { return model_.close(); }
    bool active() const { return model_.active(); }
    const QuickSettingsValues& values() const { return model_.values(); }
    void render(Display& display, uint8_t batteryPercent) const;

private:
    QuickSettingsModel model_;
};

}  // namespace pd
