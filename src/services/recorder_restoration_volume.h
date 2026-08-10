#pragma once

#include <cstdint>

namespace pd {

class RecorderRestorationVolume {
public:
    void set(uint8_t percent) { percent_ = percent; }
    uint8_t percent() const { return percent_; }

private:
    uint8_t percent_ = 0;
};

}  // namespace pd
