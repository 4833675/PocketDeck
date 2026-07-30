#include "core/input_router.h"

#include "core/mac_keymap.h"
#include "core/text_keymap.h"

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
    const bool shiftHeld = state.contains(PhysicalKey::Shift);
    const bool ctrlHeld = state.contains(PhysicalKey::Ctrl);
    const bool optHeld = state.contains(PhysicalKey::Opt);
    for (uint8_t i = 0; i < state.count; ++i) {
        const PhysicalKey key = state.keys[i];
        if (previous_.contains(key)) continue;
        if (mode == InputMode::Text || mode == InputMode::Terminal) {
            if (mode == InputMode::Text && fnHeld) {
                const InputAction action = systemAction(key, true);
                if (action == InputAction::Up || action == InputAction::Down ||
                    action == InputAction::Left || action == InputAction::Right) {
                    frame.push(action);
                    continue;
                }
            }
            if (mode == InputMode::Terminal && fnHeld) {
                InputAction action = InputAction::None;
                switch (key) {
                    case PhysicalKey::Semicolon:
                        action = optHeld ? InputAction::ScrollUp : InputAction::Up;
                        break;
                    case PhysicalKey::Comma: action = InputAction::Left; break;
                    case PhysicalKey::Period:
                        action = optHeld ? InputAction::ScrollDown : InputAction::Down;
                        break;
                    case PhysicalKey::Slash: action = InputAction::Right; break;
                    case PhysicalKey::Backtick: action = InputAction::Escape; break;
                    case PhysicalKey::Backspace: action = InputAction::DeleteForward; break;
                    case PhysicalKey::Tab: action = InputAction::QuickCommands; break;
                    default: break;
                }
                if (action != InputAction::None) {
                    frame.push(action);
                    continue;
                }
            }
            if (key == PhysicalKey::Enter) {
                frame.push(InputAction::Confirm);
            } else if (key == PhysicalKey::Backspace) {
                frame.push(InputAction::Erase);
            } else if (key == PhysicalKey::Tab) {
                frame.push(InputAction::Tab);
            } else if (fnHeld && key == PhysicalKey::Backtick) {
                frame.push(InputAction::Back);
            } else {
                char character = TextKeymap::character(key, shiftHeld);
                if (mode == InputMode::Terminal && ctrlHeld &&
                    ((character >= 'a' && character <= 'z') ||
                     (character >= 'A' && character <= 'Z'))) {
                    if (character >= 'a') character = static_cast<char>(character - 'a' + 'A');
                    character = static_cast<char>(character - 'A' + 1);
                }
                frame.pushCharacter(character);
            }
        } else {
            frame.push(systemAction(key, fnHeld));
        }
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
