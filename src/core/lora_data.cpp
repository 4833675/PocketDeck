#include "core/lora_data.h"

#include <cstring>

namespace pd {

namespace {

bool isPrintableAscii(char character) {
    const uint8_t byte = static_cast<uint8_t>(character);
    return byte >= 0x20u && byte <= 0x7eu;
}

}  // namespace

const char* loraRadioStateLabel(LoRaRadioState state) {
    switch (state) {
        case LoRaRadioState::Unavailable: return "UNAVAILABLE";
        case LoRaRadioState::Initializing: return "INITIALIZING";
        case LoRaRadioState::Listening: return "LISTENING";
        case LoRaRadioState::Transmitting: return "TRANSMITTING";
        case LoRaRadioState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* loraMessageDirectionLabel(LoRaMessageDirection direction) {
    switch (direction) {
        case LoRaMessageDirection::Rx: return "RX";
        case LoRaMessageDirection::Tx: return "TX";
    }
    return "UNKNOWN";
}

bool LoRaData::appendDraft(char character) {
    if (!isPrintableAscii(character) || draftFull()) return false;
    draft_[draftLength_++] = character;
    draft_[draftLength_] = '\0';
    return true;
}

bool LoRaData::eraseDraft() {
    if (draftEmpty()) return false;
    --draftLength_;
    draft_[draftLength_] = '\0';
    return true;
}

void LoRaData::clearDraft() {
    draftLength_ = 0;
    draft_[0] = '\0';
}

bool LoRaData::canSend() const {
    return state_ == LoRaRadioState::Listening && !draftEmpty();
}

std::size_t LoRaData::copyDraft(uint8_t* destination, std::size_t capacity) const {
    if (destination == nullptr || capacity < draftLength_) return 0;
    for (std::size_t index = 0; index < draftLength_; ++index) {
        destination[index] = static_cast<uint8_t>(draft_[index]);
    }
    return draftLength_;
}

const LoRaMessageRecord& LoRaData::historyAt(std::size_t index) const {
    static const LoRaMessageRecord emptyRecord{};
    if (index >= historySize_) return emptyRecord;
    return history_[index];
}

void LoRaData::beginInitialization() {
    state_ = LoRaRadioState::Initializing;
}

void LoRaData::beginListening() {
    state_ = LoRaRadioState::Listening;
}

bool LoRaData::beginTransmit() {
    if (!canSend()) return false;
    state_ = LoRaRadioState::Transmitting;
    return true;
}

bool LoRaData::completeTransmit(int16_t statusCode) {
    lastStatusCode_ = statusCode;
    if (state_ != LoRaRadioState::Transmitting || statusCode != 0) return false;

    appendHistory(LoRaMessageDirection::Tx,
                  reinterpret_cast<const uint8_t*>(draft_.data()), draftLength_, false);
    ++counters_.sent;
    clearDraft();
    state_ = LoRaRadioState::Listening;
    return true;
}

bool LoRaData::recordReceive(const uint8_t* bytes, std::size_t length, float rssi, float snr,
                             int16_t statusCode) {
    lastStatusCode_ = statusCode;
    state_ = LoRaRadioState::Listening;
    if (length > kLoRaPayloadLimit || (bytes == nullptr && length != 0)) {
        ++counters_.droppedPackets;
        return false;
    }

    appendHistory(LoRaMessageDirection::Rx, bytes, length, true);
    ++counters_.received;
    hasReceiveQuality_ = true;
    lastRssi_ = rssi;
    lastSnr_ = snr;
    return true;
}

void LoRaData::recordCrcFailure(int16_t statusCode) {
    lastStatusCode_ = statusCode;
    ++counters_.crcFailures;
    state_ = LoRaRadioState::Listening;
}

void LoRaData::beginRecoverableRestart(int16_t statusCode) {
    lastStatusCode_ = statusCode;
    state_ = LoRaRadioState::Initializing;
}

void LoRaData::completeRecoverableRestart(int16_t statusCode) {
    lastStatusCode_ = statusCode;
    state_ = statusCode == 0 ? LoRaRadioState::Listening : LoRaRadioState::Error;
}

void LoRaData::setPersistentError(int16_t statusCode) {
    lastStatusCode_ = statusCode;
    state_ = LoRaRadioState::Error;
}

void LoRaData::appendHistory(LoRaMessageDirection direction, const uint8_t* bytes,
                             std::size_t length, bool sanitize) {
    if (historySize_ == kLoRaHistoryCapacity) {
        std::memmove(history_.data(), history_.data() + 1,
                     (kLoRaHistoryCapacity - 1) * sizeof(LoRaMessageRecord));
        --historySize_;
    }

    LoRaMessageRecord& record = history_[historySize_++];
    record = {};
    record.direction = direction;
    record.length = length;
    for (std::size_t index = 0; index < length; ++index) {
        const uint8_t byte = bytes[index];
        record.text[index] = sanitize && (byte < 0x20u || byte > 0x7eu)
                                 ? '.'
                                 : static_cast<char>(byte);
    }
    record.text[length] = '\0';
}

}  // namespace pd
