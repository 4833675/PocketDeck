#pragma once

#include <cstdint>

namespace pd {

enum class G0Action : uint8_t {
    None,
    Home,
    QuickSettings,
};

class G0Gesture {
public:
    G0Gesture(uint32_t longPressMs, uint32_t debounceMs)
        : longPressMs_(longPressMs), debounceMs_(debounceMs) {}

    G0Action update(bool rawDown, uint32_t nowMs);
    void reset();

private:
    uint32_t longPressMs_;
    uint32_t debounceMs_;
    uint32_t rawChangedAt_ = 0;
    uint32_t pressedAt_ = 0;
    bool rawDown_ = false;
    bool stableDown_ = false;
    bool longFired_ = false;
};

}  // namespace pd

