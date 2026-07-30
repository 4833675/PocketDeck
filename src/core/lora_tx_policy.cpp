#include "core/lora_tx_policy.h"

#include <algorithm>

namespace pd {

bool LoRaTxPolicy::capture(const uint8_t* payload, std::size_t length) {
    if (active_ || payload == nullptr || length == 0 || length > payload_.size()) {
        return false;
    }

    payload_.fill(0);
    std::copy_n(payload, length, payload_.data());
    length_ = length;
    watchdogDeadlineMs_ = 0;
    active_ = true;
    watchdogArmed_ = false;
    return true;
}

void LoRaTxPolicy::clear() {
    payload_.fill(0);
    length_ = 0;
    watchdogDeadlineMs_ = 0;
    active_ = false;
    watchdogArmed_ = false;
}

bool LoRaTxPolicy::appendDraft(LoRaData& data, char character) const {
    return draftEditAllowed(data) && data.appendDraft(character);
}

bool LoRaTxPolicy::eraseDraft(LoRaData& data) const {
    return draftEditAllowed(data) && data.eraseDraft();
}

void LoRaTxPolicy::clearDraft(LoRaData& data) const {
    if (draftEditAllowed(data)) data.clearDraft();
}

void LoRaTxPolicy::armWatchdog(uint32_t nowMs, uint32_t timeOnAirUs) {
    if (!active_) return;

    const uint64_t roundedTimeOnAirMs =
        (static_cast<uint64_t>(timeOnAirUs) + 999u) / 1000u;
    const uint64_t requestedDurationMs =
        roundedTimeOnAirMs + kWatchdogSafetyMarginMs;
    const uint32_t durationMs = static_cast<uint32_t>(
        requestedDurationMs > kMaximumWatchdogDurationMs
            ? kMaximumWatchdogDurationMs
            : requestedDurationMs);
    watchdogDeadlineMs_ = nowMs + durationMs;
    watchdogArmed_ = true;
}

bool LoRaTxPolicy::watchdogExpired(uint32_t nowMs) const {
    if (!watchdogArmed_) return false;
    return static_cast<uint32_t>(nowMs - watchdogDeadlineMs_) < 0x80000000u;
}

bool LoRaTxPolicy::draftEditAllowed(const LoRaData& data) const {
    return !active_ && data.state() != LoRaRadioState::Transmitting;
}

}  // namespace pd
