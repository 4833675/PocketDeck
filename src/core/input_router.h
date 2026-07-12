#pragma once

#include "core/input.h"
#include "core/key_state.h"

namespace pd {

class InputRouter {
public:
    InputFrame update(const KeyState& state, InputMode mode, bool bleConnected);
    void reset();

private:
    static InputAction systemAction(PhysicalKey key, bool fnHeld);

    KeyState previous_{};
    InputMode mode_ = InputMode::System;
    bool initialized_ = false;
    bool wasConnected_ = false;
    bool keyboardArmed_ = false;
};

}  // namespace pd

