#include "ui/quick_settings_model.h"

namespace pd {

void QuickSettingsModel::open(QuickSettingsValues values) {
    values.brightness = adjust(values.brightness, 0);
    values.volume = adjust(values.volume, 0);
    values_ = values;
    active_ = true;
    dirty_ = false;
}

QuickSettingsResult QuickSettingsModel::handle(InputAction action) {
    if (!active_) return {};
    if (action == InputAction::Back) return close();

    QuickSettingsValues next = values_;
    switch (action) {
        case InputAction::Left: next.brightness = adjust(next.brightness, -10); break;
        case InputAction::Right: next.brightness = adjust(next.brightness, 10); break;
        case InputAction::Up: next.volume = adjust(next.volume, 10); break;
        case InputAction::Down: next.volume = adjust(next.volume, -10); break;
        case InputAction::Confirm: next.bleEnabled = !next.bleEnabled; break;
        default: return {};
    }

    if (next.brightness == values_.brightness && next.volume == values_.volume &&
        next.bleEnabled == values_.bleEnabled) {
        return {};
    }
    values_ = next;
    dirty_ = true;
    return {true, false, false};
}

QuickSettingsResult QuickSettingsModel::close() {
    if (!active_) return {};
    active_ = false;
    return {false, true, dirty_};
}

uint8_t QuickSettingsModel::adjust(uint8_t value, int delta) {
    int adjusted = static_cast<int>(value) + delta;
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 100) adjusted = 100;
    return static_cast<uint8_t>(adjusted);
}

}  // namespace pd
