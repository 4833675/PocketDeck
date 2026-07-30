#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class MediaAppEffect : uint8_t {
    None,
    Scan,
    SelectPrevious,
    SelectNext,
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

class MediaAppModel {
public:
    MediaAppEffect enter() const { return MediaAppEffect::Scan; }
    MediaAppEffect exit() const { return MediaAppEffect::StopAndGoHome; }
    MediaAppResult handle(const InputEvent& event, bool hasTracks) const;
};

}  // namespace pd
