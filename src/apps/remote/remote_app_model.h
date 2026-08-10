#pragma once

#include <cstdint>

#include "core/input.h"
#include "core/ir_data.h"

namespace pd {

enum class RemoteAppEffect : uint8_t {
    None,
    SendSony,
};

struct RemoteAppResult {
    RemoteAppEffect effect = RemoteAppEffect::None;
    SonyIrCommand command = SonyIrCommand::None;
};

class RemoteAppModel {
public:
    RemoteAppResult handle(const InputEvent& event) const;
};

}  // namespace pd
