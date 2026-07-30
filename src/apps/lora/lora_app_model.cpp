#include "apps/lora/lora_app_model.h"

namespace pd {

LoRaAppEffect LoRaAppModel::enter() {
    clearRejectedSend();
    return LoRaAppEffect::EnsureStarted;
}

LoRaAppEffect LoRaAppModel::exit() {
    clearRejectedSend();
    return LoRaAppEffect::ClearDraft;
}

LoRaAppResult LoRaAppModel::handle(const InputEvent& event, LoRaRadioState state,
                                   bool draftEmpty, uint32_t nowMs) {
    if (event.action == InputAction::Back) return {LoRaAppEffect::GoHome, '\0'};

    if (event.character >= 0x20 && event.character <= 0x7e) {
        clearRejectedSend();
        return {LoRaAppEffect::AppendDraft, event.character};
    }
    if (event.action == InputAction::Erase) {
        clearRejectedSend();
        return {LoRaAppEffect::EraseDraft, '\0'};
    }
    if (event.action != InputAction::Confirm || draftEmpty) return {};

    if (state == LoRaRadioState::Listening) {
        clearRejectedSend();
        return {LoRaAppEffect::RequestTransmit, '\0'};
    }
    if (state == LoRaRadioState::Initializing ||
        state == LoRaRadioState::Transmitting) {
        rejectSend(nowMs);
    }
    return {};
}

void LoRaAppModel::rejectSend(uint32_t nowMs) {
    rejectedAtMs_ = nowMs;
    sendRejected_ = true;
}

bool LoRaAppModel::sendRejectedVisible(uint32_t nowMs) const {
    return sendRejected_ &&
           static_cast<uint32_t>(nowMs - rejectedAtMs_) < kRejectedFeedbackDurationMs;
}

void LoRaAppModel::clearRejectedSend() {
    sendRejected_ = false;
}

}  // namespace pd
