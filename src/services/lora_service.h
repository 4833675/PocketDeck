#pragma once

#include <Module.h>
#include <modules/SX126x/SX1262.h>

#include "core/lora_data.h"

namespace pd {

class DiagnosticsService;

class LoRaService {
public:
    void ensureStarted(DiagnosticsService* diagnostics);
    void update();
    bool requestTransmit();

    LoRaData& data() { return data_; }
    const LoRaData& data() const { return data_; }

private:
    static void onDio1();

    void startHardware();
    void beginRequestedTransmit();
    void finishTransmit();
    void finishReceive();
    bool startReceive();
    bool recoverReceive(int16_t operationCode, const char* operation);
    void setPersistentError(int16_t statusCode, const char* operation);
    void prepareSharedSpi() const;
    void logState(const char* operation, int16_t statusCode,
                  std::size_t length = 0) const;

    static volatile bool dio1Fired_;

    Module module_{5, 4, 3, 6, SPI};
    SX1262 radio_{&module_};
    LoRaData data_{};
    DiagnosticsService* diagnostics_ = nullptr;
    bool startRequested_ = false;
    bool startAttempted_ = false;
    bool transmitRequested_ = false;
};

}  // namespace pd
