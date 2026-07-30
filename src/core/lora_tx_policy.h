#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/lora_data.h"

namespace pd {

class LoRaTxPolicy {
public:
    // Covers microsecond-to-millisecond rounding, main-loop jitter, and the
    // SPI setup time between the loop timestamp and actual TX launch.
    static constexpr uint32_t kWatchdogSafetyMarginMs = 1000;

    bool capture(const uint8_t* payload, std::size_t length);
    void clear();

    bool active() const { return active_; }
    bool appendDraft(LoRaData& data, char character) const;
    bool eraseDraft(LoRaData& data) const;
    void clearDraft(LoRaData& data) const;
    const uint8_t* payload() const { return payload_.data(); }
    std::size_t length() const { return length_; }

    void armWatchdog(uint32_t nowMs, uint32_t timeOnAirUs);
    bool watchdogArmed() const { return watchdogArmed_; }
    bool watchdogExpired(uint32_t nowMs) const;

private:
    static constexpr uint32_t kMaximumWatchdogDurationMs = 0x7fffffffu;

    bool draftEditAllowed(const LoRaData& data) const;

    std::array<uint8_t, kLoRaPayloadLimit> payload_{};
    std::size_t length_ = 0;
    uint32_t watchdogDeadlineMs_ = 0;
    bool active_ = false;
    bool watchdogArmed_ = false;
};

}  // namespace pd
