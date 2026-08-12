#include "apps/motion/motion_app_model.h"

namespace pd {

MotionLevelScreenAxes motionLevelScreenAxes(float rollDegrees, float pitchDegrees) {
    return {pitchDegrees, rollDegrees};
}

MotionAppResult MotionAppModel::handle(InputAction action) {
    constexpr uint8_t kPageCount = 3;
    uint8_t index = static_cast<uint8_t>(page_);
    if (action == InputAction::Left) {
        page_ = static_cast<MotionPage>((index + kPageCount - 1) % kPageCount);
        return {};
    }
    if (action == InputAction::Right || action == InputAction::Tab) {
        page_ = static_cast<MotionPage>((index + 1) % kPageCount);
        return {};
    }
    if (action == InputAction::Back) return {MotionAppEffect::GoHome};
    if (action != InputAction::Confirm) return {};

    switch (page_) {
        case MotionPage::Level: return {MotionAppEffect::ZeroLevel};
        case MotionPage::Activity: return {MotionAppEffect::ResetPeak};
        default: return {};
    }
}

}  // namespace pd
