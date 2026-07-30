#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

constexpr std::size_t kLoRaPayloadLimit = 120;
constexpr std::size_t kLoRaHistoryCapacity = 6;

enum class LoRaRadioState : uint8_t {
    Unavailable,
    Initializing,
    Listening,
    Transmitting,
    Error,
};

enum class LoRaMessageDirection : uint8_t {
    Rx,
    Tx,
};

struct LoRaMessageRecord {
    LoRaMessageDirection direction = LoRaMessageDirection::Rx;
    std::size_t length = 0;
    std::array<char, kLoRaPayloadLimit + 1> text{};
};

struct LoRaCounters {
    uint32_t sent = 0;
    uint32_t received = 0;
    uint32_t crcFailures = 0;
    uint32_t droppedPackets = 0;
};

const char* loraRadioStateLabel(LoRaRadioState state);
const char* loraMessageDirectionLabel(LoRaMessageDirection direction);

class LoRaData {
public:
    LoRaRadioState state() const { return state_; }
    const LoRaCounters& counters() const { return counters_; }
    int16_t lastStatusCode() const { return lastStatusCode_; }

    bool appendDraft(char character);
    bool eraseDraft();
    void clearDraft();
    const char* draft() const { return draft_.data(); }
    std::size_t draftLength() const { return draftLength_; }
    bool draftEmpty() const { return draftLength_ == 0; }
    bool draftFull() const { return draftLength_ == kLoRaPayloadLimit; }
    bool canSend() const;
    std::size_t copyDraft(uint8_t* destination, std::size_t capacity) const;

    std::size_t historySize() const { return historySize_; }
    const LoRaMessageRecord& historyAt(std::size_t index) const;

    bool hasReceiveQuality() const { return hasReceiveQuality_; }
    float lastRssi() const { return lastRssi_; }
    float lastSnr() const { return lastSnr_; }

    void beginInitialization();
    void beginListening();
    bool beginTransmit();
    bool completeTransmit(int16_t statusCode);
    bool recordReceive(const uint8_t* bytes, std::size_t length, float rssi, float snr,
                       int16_t statusCode);
    void recordCrcFailure(int16_t statusCode);
    void beginRecoverableRestart(int16_t statusCode);
    void completeRecoverableRestart(int16_t statusCode);
    void setPersistentError(int16_t statusCode);

private:
    void appendHistory(LoRaMessageDirection direction, const uint8_t* bytes,
                       std::size_t length, bool sanitize);

    LoRaRadioState state_ = LoRaRadioState::Unavailable;
    LoRaCounters counters_{};
    int16_t lastStatusCode_ = 0;
    std::array<char, kLoRaPayloadLimit + 1> draft_{};
    std::size_t draftLength_ = 0;
    std::array<LoRaMessageRecord, kLoRaHistoryCapacity> history_{};
    std::size_t historySize_ = 0;
    bool hasReceiveQuality_ = false;
    float lastRssi_ = 0.0f;
    float lastSnr_ = 0.0f;
};

}  // namespace pd
