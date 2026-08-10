#include "services/ir_service.h"

#include <Arduino.h>

#define DISABLE_CODE_FOR_RECEIVER
#include <IRremote.hpp>

namespace pd {
namespace {

constexpr uint8_t kIrSendPin = 44;

}  // namespace

void IrService::setActive(bool active) {
    if (active == active_) return;

    if (active) {
        IrSender.begin(DISABLE_LED_FEEDBACK);
        IrSender.setSendPin(kIrSendPin);
        active_ = true;
        return;
    }

    pinMode(kIrSendPin, OUTPUT);
    digitalWrite(kIrSendPin, LOW);
    active_ = false;
}

bool IrService::active() const {
    return active_;
}

bool IrService::send(SonyIrCommand command) {
    if (!active_) return false;

    const SonyIrCode* code = sonyIrCodeFor(command);
    if (code == nullptr) return false;

    IrSender.sendSony(code->device, code->command, code->repeats, code->bits);
    return true;
}

}  // namespace pd
