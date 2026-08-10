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
    Remote,
    LoRa,
    Media,
    Recorder,
    Weather,
    Settings,
};

}  // namespace pd
