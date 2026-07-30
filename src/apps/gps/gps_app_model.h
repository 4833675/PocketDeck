#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class GpsPage : uint8_t {
    Position,
    Time,
    Receiver,
    Motion,
};

class GpsAppModel {
public:
    GpsPage page() const { return page_; }
    void reset();
    void handle(InputAction action);

private:
    GpsPage page_ = GpsPage::Position;
};

}  // namespace pd
