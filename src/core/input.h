#pragma once

#include <array>
#include <cstdint>

#include "core/hid_report.h"

namespace pd {

enum class InputMode : uint8_t {
    System,
    Keyboard,
};

enum class InputAction : uint8_t {
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Tab,
};

struct InputEvent {
    InputAction action = InputAction::None;
};

struct InputFrame {
    static constexpr uint8_t kEventCapacity = 8;

    std::array<InputEvent, kEventCapacity> events{};
    uint8_t eventCount = 0;
    bool hasHidReport = false;
    HidReport hidReport{};

    void push(InputAction action) {
        if (action == InputAction::None || eventCount == kEventCapacity) return;
        events[eventCount++].action = action;
    }
};

}  // namespace pd

