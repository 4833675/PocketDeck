#pragma once

#include <cstdint>

#include "core/key_state.h"

namespace pd {

class Board {
public:
    bool begin();
    void update();
    KeyState keyState() const;
    bool g0Down() const;
    uint8_t batteryPercent() const;
    void setBrightness(uint8_t percent);
    void setVolume(uint8_t percent);
};

}  // namespace pd

