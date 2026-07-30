#include "apps/media/media_app_model.h"

namespace pd {

MediaAppResult MediaAppModel::handle(const InputEvent& event,
                                     const MediaAppInputState& state) const {
    if (event.action == InputAction::Back || event.action == InputAction::Erase) {
        return {state.atRootDirectory ? MediaAppEffect::StopAndGoHome
                                      : MediaAppEffect::GoParentDirectory,
                0};
    }
    if (event.action == InputAction::Tab) return {MediaAppEffect::Scan, 0};
    if (event.character == '-' || event.character == '_') {
        return {MediaAppEffect::AdjustVolume, -5};
    }
    if (event.character == '=' || event.character == '+') {
        return {MediaAppEffect::AdjustVolume, 5};
    }
    if (!state.hasEntries) return {};

    switch (event.action) {
        case InputAction::Up: return {MediaAppEffect::SelectPrevious, 0};
        case InputAction::Down: return {MediaAppEffect::SelectNext, 0};
        case InputAction::Left:
            return {state.hasPlayableTrack ? MediaAppEffect::PlayPrevious
                                           : MediaAppEffect::None,
                    0};
        case InputAction::Right:
            return {state.hasPlayableTrack ? MediaAppEffect::PlayNext
                                           : MediaAppEffect::None,
                    0};
        case InputAction::Confirm:
            return {state.selectedDirectory ? MediaAppEffect::OpenSelectedDirectory
                                            : MediaAppEffect::ToggleSelected,
                    0};
        default: return {};
    }
}

}  // namespace pd
