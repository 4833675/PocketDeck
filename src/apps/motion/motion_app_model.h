#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class MotionPage : uint8_t {
    Live,
    Level,
    Activity,
};

enum class MotionAppEffect : uint8_t {
    None,
    ZeroLevel,
    ResetPeak,
    GoHome,
};

struct MotionAppResult {
    MotionAppEffect effect = MotionAppEffect::None;
};

class MotionAppModel {
public:
    MotionPage page() const { return page_; }
    MotionAppResult handle(InputAction action);

private:
    MotionPage page_ = MotionPage::Live;
};

}  // namespace pd
