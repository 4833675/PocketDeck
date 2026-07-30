#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class MediaAppEffect : uint8_t {
    None,
    Scan,
    SelectPrevious,
    SelectNext,
    OpenSelectedDirectory,
    GoParentDirectory,
    ToggleSelected,
    PlayPrevious,
    PlayNext,
    AdjustVolume,
    StopAndGoHome,
};

struct MediaAppResult {
    MediaAppEffect effect = MediaAppEffect::None;
    int8_t volumeDelta = 0;
};

struct MediaAppInputState {
    bool hasEntries = false;
    bool hasPlayableTrack = false;
    bool selectedDirectory = false;
    bool atRootDirectory = true;
};

class MediaAppModel {
public:
    MediaAppEffect enter() const { return MediaAppEffect::Scan; }
    MediaAppEffect exit() const { return MediaAppEffect::StopAndGoHome; }
    MediaAppResult handle(const InputEvent& event, const MediaAppInputState& state) const;
};

}  // namespace pd
