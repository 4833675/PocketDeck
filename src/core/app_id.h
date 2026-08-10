#pragma once

#include <cstdint>

namespace pd {

enum class AppId : uint8_t {
    None,
    Launcher,
    Keyboard,
    Ssh,
    Gps,
    Motion,
    LoRa,
    Media,
    Weather,
    Settings,
};

}  // namespace pd
