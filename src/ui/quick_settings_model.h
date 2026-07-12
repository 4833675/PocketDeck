#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

struct QuickSettingsValues {
    uint8_t brightness = 78;
    uint8_t volume = 55;
    bool bleEnabled = true;
};

struct QuickSettingsResult {
    bool valuesChanged = false;
    bool closed = false;
    bool persist = false;
};

class QuickSettingsModel {
public:
    void open(QuickSettingsValues values);
    QuickSettingsResult handle(InputAction action);
    QuickSettingsResult close();

    bool active() const { return active_; }
    bool dirty() const { return dirty_; }
    const QuickSettingsValues& values() const { return values_; }

private:
    static uint8_t adjust(uint8_t value, int delta);

    QuickSettingsValues values_{};
    bool active_ = false;
    bool dirty_ = false;
};

}  // namespace pd
