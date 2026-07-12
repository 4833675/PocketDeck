#include "core/g0_gesture.h"

namespace pd {

G0Action G0Gesture::update(bool rawDown, uint32_t nowMs) {
    if (rawDown != rawDown_) {
        rawDown_ = rawDown;
        rawChangedAt_ = nowMs;
    }

    if (rawDown_ != stableDown_ && nowMs - rawChangedAt_ >= debounceMs_) {
        stableDown_ = rawDown_;
        if (stableDown_) {
            pressedAt_ = nowMs;
            longFired_ = false;
        } else if (!longFired_) {
            return G0Action::Home;
        }
    }

    if (stableDown_ && !longFired_ && nowMs - pressedAt_ >= longPressMs_) {
        longFired_ = true;
        return G0Action::QuickSettings;
    }

    return G0Action::None;
}

void G0Gesture::reset() {
    rawChangedAt_ = 0;
    pressedAt_ = 0;
    rawDown_ = false;
    stableDown_ = false;
    longFired_ = false;
}

}  // namespace pd
