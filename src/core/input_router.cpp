#include "core/input_router.h"

#include "core/mac_keymap.h"

namespace pd {

InputAction InputRouter::systemAction(PhysicalKey key, bool fnHeld) {
    if (fnHeld) {
        switch (key) {
            case PhysicalKey::Semicolon: return InputAction::Up;
            case PhysicalKey::Comma: return InputAction::Left;
            case PhysicalKey::Period: return InputAction::Down;
            case PhysicalKey::Slash: return InputAction::Right;
            default: break;
        }
    }

    switch (key) {
        case PhysicalKey::Enter: return InputAction::Confirm;
        case PhysicalKey::Backspace: return InputAction::Back;
        case PhysicalKey::Tab: return InputAction::Tab;
        default: return InputAction::None;
    }
}

InputFrame InputRouter::update(const KeyState& state, InputMode mode, bool bleConnected) {
    InputFrame frame;
    const bool modeChanged = initialized_ && mode != mode_;

    if (!initialized_) {
        initialized_ = true;
        mode_ = mode;
    } else if (modeChanged) {
        mode_ = mode;
    }

    if (mode == InputMode::Keyboard) {
        if (modeChanged || (!wasConnected_ && bleConnected)) keyboardArmed_ = false;

        if (!bleConnected) {
            wasConnected_ = false;
            previous_ = state;
            return frame;
        }

        if (!keyboardArmed_) {
            if (state.count == 0) {
                keyboardArmed_ = true;
                frame.hasHidReport = true;
            }
            wasConnected_ = true;
            previous_ = state;
            return frame;
        }

        frame.hasHidReport = true;
        frame.hidReport = MacKeymap::buildReport(state);
        wasConnected_ = true;
        previous_ = state;
        return frame;
    }

    keyboardArmed_ = false;
    wasConnected_ = false;

    if (modeChanged) {
        previous_ = state;
        return frame;
    }

    const bool fnHeld = state.contains(PhysicalKey::Fn);
    for (uint8_t i = 0; i < state.count; ++i) {
        const PhysicalKey key = state.keys[i];
        if (previous_.contains(key)) continue;
        frame.push(systemAction(key, fnHeld));
    }
    previous_ = state;
    return frame;
}

void InputRouter::reset() {
    previous_ = KeyState{};
    initialized_ = false;
    wasConnected_ = false;
    keyboardArmed_ = false;
    mode_ = InputMode::System;
}

}  // namespace pd

