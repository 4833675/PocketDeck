#include "apps/launcher/launcher_model.h"

namespace pd {

AppId LauncherModel::handle(InputAction action) {
    if (action == InputAction::Left) {
        index_ = static_cast<uint8_t>((index_ + kApps.size() - 1) % kApps.size());
    } else if (action == InputAction::Right) {
        index_ = static_cast<uint8_t>((index_ + 1) % kApps.size());
    } else if (action == InputAction::Confirm) {
        return selected();
    }
    return AppId::None;
}

}  // namespace pd
