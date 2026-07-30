#include "apps/gps/gps_app_model.h"

namespace pd {

void GpsAppModel::reset() {
    page_ = GpsPage::Position;
}

void GpsAppModel::handle(InputAction action) {
    constexpr uint8_t kPageCount = 4;
    uint8_t index = static_cast<uint8_t>(page_);
    if (action == InputAction::Left) {
        index = static_cast<uint8_t>((index + kPageCount - 1) % kPageCount);
    } else if (action == InputAction::Right || action == InputAction::Tab) {
        index = static_cast<uint8_t>((index + 1) % kPageCount);
    } else {
        return;
    }
    page_ = static_cast<GpsPage>(index);
}

}  // namespace pd
