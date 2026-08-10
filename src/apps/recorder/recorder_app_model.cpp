#include "apps/recorder/recorder_app_model.h"

namespace pd {

RecorderAppResult RecorderAppModel::handle(const InputEvent& event,
                                           const RecorderAppInputState& state) {
    if (page_ == RecorderPage::DeleteConfirm) {
        if (event.action == InputAction::Erase) {
            page_ = RecorderPage::Files;
        } else if (event.action == InputAction::Confirm && state.hasEntries) {
            page_ = RecorderPage::Files;
            return {RecorderAppEffect::DeleteSelected};
        }
        return {};
    }

    if (event.action == InputAction::Back || event.action == InputAction::Erase) {
        return {RecorderAppEffect::GoHome};
    }

    if (event.action == InputAction::Tab) {
        if (!state.recording && !state.playing) {
            page_ = page_ == RecorderPage::Record ? RecorderPage::Files : RecorderPage::Record;
        }
        return {};
    }

    if (page_ == RecorderPage::Record) {
        if (event.action != InputAction::Confirm) return {};
        if (state.recording) return {RecorderAppEffect::StopRecording};
        if (!state.playing) return {RecorderAppEffect::StartRecording};
        return {};
    }

    if (event.action == InputAction::Up && state.hasEntries) {
        return {RecorderAppEffect::SelectPrevious};
    }
    if (event.action == InputAction::Down && state.hasEntries) {
        return {RecorderAppEffect::SelectNext};
    }
    if (event.action == InputAction::Confirm && state.hasEntries) {
        return {state.playing ? RecorderAppEffect::StopPlayback
                              : RecorderAppEffect::StartPlayback};
    }
    if (event.action == InputAction::None && event.character == 'd' && state.hasEntries) {
        page_ = RecorderPage::DeleteConfirm;
    }
    return {};
}

}  // namespace pd
