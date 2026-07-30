#pragma once

#include <cstdint>

#include "core/input.h"
#include "core/lora_data.h"

namespace pd {

enum class LoRaAppEffect : uint8_t {
    None,
    EnsureStarted,
    ClearDraft,
    AppendDraft,
    EraseDraft,
    RequestTransmit,
    GoHome,
};

struct LoRaAppResult {
    LoRaAppEffect effect = LoRaAppEffect::None;
    char character = '\0';
};

class LoRaAppModel {
public:
    static constexpr uint32_t kRejectedFeedbackDurationMs = 1000;

    LoRaAppEffect enter();
    LoRaAppEffect exit();
    LoRaAppResult handle(const InputEvent& event, LoRaRadioState state,
                         bool draftEmpty, uint32_t nowMs);
    void rejectSend(uint32_t nowMs);
    bool sendRejectedVisible(uint32_t nowMs) const;

private:
    void clearRejectedSend();

    uint32_t rejectedAtMs_ = 0;
    bool sendRejected_ = false;
};

}  // namespace pd
