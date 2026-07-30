#include "services/lora_service.h"

#include <array>

#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <utility/PI4IOE5V6408_Class.hpp>

#include "services/diagnostics_service.h"

namespace pd {
namespace {

constexpr int8_t kSdCsPin = 12;
constexpr int8_t kLoRaCsPin = 5;
constexpr uint8_t kAntennaSwitchAddress = 0x43;
constexpr uint8_t kAntennaSwitchPin = 0;
constexpr uint32_t kAntennaSwitchFrequency = 400000;

constexpr float kFrequencyMhz = 868.0F;
constexpr float kBandwidthKhz = 125.0F;
constexpr uint8_t kSpreadingFactor = 12;
constexpr uint8_t kCodingRateDenominator = 5;
constexpr uint8_t kSyncWord = 0x34;
constexpr int8_t kOutputPowerDbm = 22;
constexpr uint16_t kPreambleSymbols = 20;
constexpr float kTcxoVoltage = 3.0F;
constexpr float kCurrentLimitMa = 140.0F;

}  // namespace

volatile bool LoRaService::dio1Fired_ = false;

void LoRaService::ensureStarted(DiagnosticsService* diagnostics) {
    if (diagnostics != nullptr) diagnostics_ = diagnostics;
    startRequested_ = true;
}

void LoRaService::update() {
    if (startRequested_ && !startAttempted_) startHardware();
    if (data_.state() == LoRaRadioState::Error ||
        data_.state() == LoRaRadioState::Unavailable) {
        transmitRequested_ = false;
        return;
    }

    if (consumeDio1Flag()) handleRadioIrq();

    if (transmitRequested_) beginRequestedTransmit();
}

bool LoRaService::appendDraft(char character) {
    return data_.appendDraft(character);
}

bool LoRaService::eraseDraft() {
    return data_.eraseDraft();
}

void LoRaService::clearDraft() {
    data_.clearDraft();
}

bool LoRaService::requestTransmit() {
    if (transmitRequested_ || !data_.canSend()) return false;
    transmitRequested_ = true;
    return true;
}

void IRAM_ATTR LoRaService::onDio1() {
    dio1Fired_ = true;
}

void LoRaService::startHardware() {
    startAttempted_ = true;
    prepareSharedSpi();

    m5::PI4IOE5V6408_Class antennaSwitch(kAntennaSwitchAddress,
                                         kAntennaSwitchFrequency, &m5::In_I2C);
    if (!antennaSwitch.begin()) {
        logState("antenna-unavailable", RADIOLIB_ERR_NONE);
        return;
    }
    antennaSwitch.setDirection(kAntennaSwitchPin, true);
    antennaSwitch.setHighImpedance(kAntennaSwitchPin, false);
    antennaSwitch.digitalWrite(kAntennaSwitchPin, true);

    data_.beginInitialization();
    prepareSharedSpi();
    int16_t status = radio_.begin(kFrequencyMhz, kBandwidthKhz, kSpreadingFactor,
                                  kCodingRateDenominator, kSyncWord, kOutputPowerDbm,
                                  kPreambleSymbols, kTcxoVoltage, true);
    if (status != RADIOLIB_ERR_NONE) {
        setPersistentError(status, "begin");
        return;
    }

    prepareSharedSpi();
    status = radio_.setCurrentLimit(kCurrentLimitMa);
    if (status != RADIOLIB_ERR_NONE) {
        setPersistentError(status, "current-limit");
        return;
    }

    attachDio1();
    if (startReceive()) logState("started", RADIOLIB_ERR_NONE);
}

void LoRaService::beginRequestedTransmit() {
    transmitRequested_ = false;
    if (!data_.canSend() || !reconcileReceiveBeforeTransmit()) return;
    if (!data_.beginTransmit()) return;

    std::array<uint8_t, kLoRaPayloadLimit> payload{};
    const std::size_t length = data_.copyDraft(payload.data(), payload.size());
    if (length == 0) {
        setPersistentError(RADIOLIB_ERR_UNKNOWN, "tx-copy");
        return;
    }

    prepareSharedSpi();
    const int16_t status = radio_.startTransmit(payload.data(), length);
    if (status != RADIOLIB_ERR_NONE) {
        recoverReceive(status, "tx-start");
        return;
    }
    logState("tx-started", status, length);
}

bool LoRaService::reconcileReceiveBeforeTransmit() {
    // Quiesce RX before changing the model state. Any packet that completed
    // before standby is reconciled while it is still unambiguously an RX.
    detachDio1();
    consumeDio1Flag();

    prepareSharedSpi();
    const int16_t standbyStatus = radio_.standby();
    if (standbyStatus != RADIOLIB_ERR_NONE) {
        recoverReceive(standbyStatus, "tx-gate-standby");
        return false;
    }

    prepareSharedSpi();
    const uint32_t irqFlags = radio_.getIrqFlags();
    if ((irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) != 0) {
        if (!finishReceive(false)) return false;
    } else {
        prepareSharedSpi();
        const int16_t clearStatus = radio_.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
        if (clearStatus != RADIOLIB_ERR_NONE) {
            recoverReceive(clearStatus, "tx-gate-clear");
            return false;
        }
    }

    consumeDio1Flag();
    attachDio1();
    return data_.state() == LoRaRadioState::Listening;
}

void LoRaService::handleRadioIrq() {
    // The software flag is only a wakeup. Hardware provenance must match the
    // active operation before either completion path is allowed to mutate data.
    prepareSharedSpi();
    const uint32_t irqFlags = radio_.getIrqFlags();
    const LoRaRadioState state = data_.state();

    if (state == LoRaRadioState::Transmitting &&
        (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) != 0) {
        finishTransmit();
        return;
    }
    if (state == LoRaRadioState::Listening &&
        (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) != 0) {
        finishReceive(true);
        return;
    }

    if ((irqFlags & RADIOLIB_SX126X_IRQ_TIMEOUT) != 0) {
        recoverReceive(state == LoRaRadioState::Transmitting
                           ? RADIOLIB_ERR_TX_TIMEOUT
                           : RADIOLIB_ERR_RX_TIMEOUT,
                       "irq-timeout");
        return;
    }

    prepareSharedSpi();
    const int16_t clearStatus = radio_.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
    if (clearStatus != RADIOLIB_ERR_NONE) {
        recoverReceive(clearStatus, "irq-clear");
        return;
    }

    logState("irq-stale", RADIOLIB_ERR_NONE);
    if (state == LoRaRadioState::Listening) startReceive();
}

void LoRaService::finishTransmit() {
    const std::size_t length = data_.draftLength();
    prepareSharedSpi();
    const int16_t status = radio_.finishTransmit();
    if (status != RADIOLIB_ERR_NONE) {
        recoverReceive(status, "tx-finish");
        return;
    }

    data_.completeTransmit(status);
    logState("tx-complete", status, length);
    startReceive();
}

bool LoRaService::finishReceive(bool rearm) {
    prepareSharedSpi();
    const std::size_t length = radio_.getPacketLength();
    std::array<uint8_t, kLoRaPayloadLimit> payload{};
    const std::size_t readLength = length <= payload.size() ? length : payload.size();

    prepareSharedSpi();
    const int16_t status = radio_.readData(payload.data(), readLength);
    if (length > kLoRaPayloadLimit) {
        data_.recordReceive(nullptr, length, 0.0F, 0.0F, status);
        if (status == RADIOLIB_ERR_CRC_MISMATCH) data_.recordCrcFailure(status);
        logState("rx-dropped", status, length);
        if (status != RADIOLIB_ERR_NONE && status != RADIOLIB_ERR_CRC_MISMATCH) {
            recoverReceive(status, "rx-read");
            return false;
        }
        return !rearm || startReceive();
    }

    if (status == RADIOLIB_ERR_CRC_MISMATCH) {
        data_.recordCrcFailure(status);
        logState("rx-crc", status, length);
        return !rearm || startReceive();
    }
    if (status != RADIOLIB_ERR_NONE) {
        recoverReceive(status, "rx-read");
        return false;
    }

    prepareSharedSpi();
    const float rssi = radio_.getRSSI();
    const float snr = radio_.getSNR();
    data_.recordReceive(payload.data(), length, rssi, snr, status);
    logState("rx-complete", status, length);
    return !rearm || startReceive();
}

bool LoRaService::startReceive() {
    prepareSharedSpi();
    const int16_t status = radio_.startReceive();
    if (status != RADIOLIB_ERR_NONE) return recoverReceive(status, "rx-start");
    data_.beginListening();
    return true;
}

bool LoRaService::recoverReceive(int16_t operationCode, const char* operation) {
    transmitRequested_ = false;
    detachDio1();
    consumeDio1Flag();
    data_.beginRecoverableRestart(operationCode);
    logState(operation, operationCode);

    prepareSharedSpi();
    int16_t status = radio_.standby();
    if (status == RADIOLIB_ERR_NONE) {
        prepareSharedSpi();
        status = radio_.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
    }
    if (status == RADIOLIB_ERR_NONE) {
        consumeDio1Flag();
        attachDio1();
        prepareSharedSpi();
        status = radio_.startReceive();
    }
    data_.completeRecoverableRestart(status);
    if (status != RADIOLIB_ERR_NONE) {
        detachDio1();
        consumeDio1Flag();
        logState("recovery-failed", status);
        return false;
    }
    logState("recovered", status);
    return true;
}

void LoRaService::setPersistentError(int16_t statusCode, const char* operation) {
    transmitRequested_ = false;
    detachDio1();
    consumeDio1Flag();
    data_.setPersistentError(statusCode);
    logState(operation, statusCode);
}

bool LoRaService::consumeDio1Flag() {
    noInterrupts();
    const bool fired = dio1Fired_;
    dio1Fired_ = false;
    interrupts();
    return fired;
}

void LoRaService::attachDio1() {
    radio_.setDio1Action(&LoRaService::onDio1);
}

void LoRaService::detachDio1() {
    radio_.clearDio1Action();
}

void LoRaService::prepareSharedSpi() const {
    // SdLogService owns the sole SPI.begin(40, 39, 14, 12) call during boot,
    // even when no card is present. The explicit-SPI Module constructor above
    // therefore reuses that bus without letting RadioLib begin or end it.
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);
    pinMode(kLoRaCsPin, OUTPUT);
    digitalWrite(kLoRaCsPin, HIGH);
}

void LoRaService::logState(const char* operation, int16_t statusCode,
                           std::size_t length) const {
    if (diagnostics_ == nullptr) return;
    const LoRaCounters& counters = data_.counters();
    diagnostics_->logf(
        "LoRa %s state=%s code=%d len=%u tx=%lu rx=%lu crc=%lu drop=%lu",
        operation != nullptr ? operation : "event", loraRadioStateLabel(data_.state()),
        static_cast<int>(statusCode), static_cast<unsigned>(length),
        static_cast<unsigned long>(counters.sent),
        static_cast<unsigned long>(counters.received),
        static_cast<unsigned long>(counters.crcFailures),
        static_cast<unsigned long>(counters.droppedPackets));
}

}  // namespace pd
