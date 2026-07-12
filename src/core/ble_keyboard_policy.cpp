#include "core/ble_keyboard_policy.h"

namespace pd {

void BleKeyboardPolicy::setConnected(bool connected) {
    if (connected == connected_) return;
    connected_ = connected;
    releasePending_ = connected;
    hasLastReport_ = false;
    lastReport_ = HidReport{};
}

ReportDecision BleKeyboardPolicy::nextReport(const HidReport& requested) {
    if (!connected_) return {};

    if (releasePending_) {
        releasePending_ = false;
        hasLastReport_ = true;
        lastReport_ = HidReport{};
        return {true, HidReport{}};
    }

    if (hasLastReport_ && requested == lastReport_) return {};
    hasLastReport_ = true;
    lastReport_ = requested;
    return {true, requested};
}

}  // namespace pd
