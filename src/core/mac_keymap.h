#pragma once

#include "core/hid_report.h"
#include "core/key_state.h"

namespace pd {

class MacKeymap {
public:
    static HidReport buildReport(const KeyState& state);
};

}  // namespace pd

