#pragma once

#include <cstdint>

namespace pd {

enum class AppId : uint8_t {
    None,
    Launcher,
    Keyboard,
    Gps,
    Settings,
};

}  // namespace pd
