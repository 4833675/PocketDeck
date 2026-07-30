#include "apps/media/media_app_model.h"

namespace pd {

MediaAppResult MediaAppModel::handle(const InputEvent& event, bool hasTracks) const {
    if (event.action == InputAction::Back || event.action == InputAction::Erase) {
        return {MediaAppEffect::StopAndGoHome, 0};
    }
    if (event.action == InputAction::Tab) return {MediaAppEffect::Scan, 0};
    if (event.character == '-' || event.character == '_') {
        return {MediaAppEffect::AdjustVolume, -5};
    }
    if (event.character == '=' || event.character == '+') {
        return {MediaAppEffect::AdjustVolume, 5};
    }
    if (!hasTracks) return {};

    switch (event.action) {
        case InputAction::Up: return {MediaAppEffect::SelectPrevious, 0};
        case InputAction::Down: return {MediaAppEffect::SelectNext, 0};
        case InputAction::Left: return {MediaAppEffect::PlayPrevious, 0};
        case InputAction::Right: return {MediaAppEffect::PlayNext, 0};
        case InputAction::Confirm: return {MediaAppEffect::ToggleSelected, 0};
        default: return {};
    }
}

}  // namespace pd
