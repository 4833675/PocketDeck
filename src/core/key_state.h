#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>

#include "core/physical_key.h"

namespace pd {

struct KeyState {
    static constexpr uint8_t kCapacity = 8;

    std::array<PhysicalKey, kCapacity> keys{};
    uint8_t count = 0;

    KeyState() = default;

    KeyState(std::initializer_list<PhysicalKey> initial) {
        for (const PhysicalKey key : initial) {
            if (count == kCapacity) break;
            keys[count++] = key;
        }
    }

    bool contains(PhysicalKey wanted) const {
        for (uint8_t i = 0; i < count; ++i) {
            if (keys[i] == wanted) return true;
        }
        return false;
    }
};

}  // namespace pd

