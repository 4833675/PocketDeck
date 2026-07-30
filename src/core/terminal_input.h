#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/input.h"

namespace pd {

struct TerminalInput {
    static constexpr std::size_t kCapacity = 8;

    std::array<uint8_t, kCapacity> bytes{};
    std::size_t length = 0;

    static TerminalInput from(const char* sequence);
    bool empty() const { return length == 0; }
    const uint8_t* data() const { return bytes.data(); }

    bool operator==(const TerminalInput& other) const;
    bool operator!=(const TerminalInput& other) const { return !(*this == other); }
};

TerminalInput encodeTerminalInput(const InputEvent& event);

}  // namespace pd
