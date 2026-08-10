#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class RecorderPage : uint8_t {
    Record,
    Files,
    DeleteConfirm,
};

enum class RecorderAppEffect : uint8_t {
    None,
    SelectPrevious,
    SelectNext,
    StartRecording,
    StopRecording,
    StartPlayback,
    StopPlayback,
    DeleteSelected,
    GoHome,
    Cleanup,
};

struct RecorderAppResult {
    RecorderAppEffect effect = RecorderAppEffect::None;
};

struct RecorderAppInputState {
    bool hasEntries = false;
    bool recording = false;
    bool playing = false;
};

class RecorderAppModel {
public:
    RecorderAppResult handle(const InputEvent& event, const RecorderAppInputState& state);
    RecorderAppEffect exit();
    RecorderPage page() const { return page_; }
    InputMode inputMode() const { return InputMode::Text; }

private:
    RecorderPage page_ = RecorderPage::Record;
};

}  // namespace pd
