#pragma once

#include <cstdint>

namespace pd {

enum class SonyIrCommand : uint8_t {
    None,
    Up,
    Down,
    Left,
    Right,
    Ok,
    Back,
    Return,
    Power,
    Home,
    Input,
    Mute,
    VolumeDown,
    VolumeUp,
};

struct SonyIrCode {
    uint8_t device = 0;
    uint8_t command = 0;
    uint8_t bits = 0;
    uint8_t repeats = 0;
};

const SonyIrCode* sonyIrCodeFor(SonyIrCommand command);

}  // namespace pd
