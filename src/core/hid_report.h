#pragma once

#include <array>
#include <cstdint>

namespace pd {

struct HidReport {
    uint8_t modifier = 0;
    uint8_t reserved = 0;
    std::array<uint8_t, 6> keys{};

    static HidReport single(uint8_t modifierMask, uint8_t usage) {
        HidReport report;
        report.modifier = modifierMask;
        if (usage != 0) report.keys[0] = usage;
        return report;
    }

    bool push(uint8_t usage) {
        if (usage == 0) return true;
        for (uint8_t& key : keys) {
            if (key == 0) {
                key = usage;
                return true;
            }
        }
        keys.fill(0x01);
        return false;
    }

    bool empty() const {
        if (modifier != 0) return false;
        for (const uint8_t key : keys) {
            if (key != 0) return false;
        }
        return true;
    }

    std::array<uint8_t, 8> bytes() const {
        return {modifier, reserved, keys[0], keys[1], keys[2], keys[3], keys[4], keys[5]};
    }

    friend bool operator==(const HidReport& left, const HidReport& right) {
        return left.modifier == right.modifier && left.reserved == right.reserved &&
               left.keys == right.keys;
    }

    friend bool operator!=(const HidReport& left, const HidReport& right) {
        return !(left == right);
    }
};

}  // namespace pd

