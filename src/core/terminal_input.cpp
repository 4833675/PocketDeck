#include "core/terminal_input.h"

#include <cstring>

namespace pd {

TerminalInput TerminalInput::from(const char* sequence) {
    TerminalInput input;
    if (sequence == nullptr) return input;
    const std::size_t length = std::strlen(sequence);
    input.length = length < input.bytes.size() ? length : input.bytes.size();
    std::memcpy(input.bytes.data(), sequence, input.length);
    return input;
}

bool TerminalInput::operator==(const TerminalInput& other) const {
    return length == other.length &&
           std::memcmp(bytes.data(), other.bytes.data(), length) == 0;
}

TerminalInput encodeTerminalInput(const InputEvent& event) {
    if (event.character != '\0') {
        TerminalInput input;
        input.bytes[0] = static_cast<uint8_t>(event.character);
        input.length = 1;
        return input;
    }
    switch (event.action) {
        case InputAction::Confirm: return TerminalInput::from("\r");
        case InputAction::Erase: return TerminalInput::from("\x7f");
        case InputAction::Tab: return TerminalInput::from("\t");
        case InputAction::Up: return TerminalInput::from("\x1b[A");
        case InputAction::Down: return TerminalInput::from("\x1b[B");
        case InputAction::Right: return TerminalInput::from("\x1b[C");
        case InputAction::Left: return TerminalInput::from("\x1b[D");
        case InputAction::Escape: return TerminalInput::from("\x1b");
        case InputAction::DeleteForward: return TerminalInput::from("\x1b[3~");
        default: return {};
    }
}

}  // namespace pd
