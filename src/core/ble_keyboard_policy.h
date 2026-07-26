#pragma once

#include "core/hid_report.h"

namespace pd {

struct ReportDecision {
    bool send = false;
    HidReport report{};
};

class BleKeyboardPolicy {
public:
    // A physical BLE connection is not an identity check: bonded centrals may
    // reconnect with a resolvable private address. Let the stack resolve the
    // existing bond and only block a *new pairing* while a bond is stored.
    static bool newPairingAllowed(bool hasStoredBond) { return !hasStoredBond; }

    static bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
        return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
    }

    void setConnected(bool connected);
    ReportDecision nextReport(const HidReport& requested);

private:
    bool connected_ = false;
    bool releasePending_ = false;
    bool hasLastReport_ = false;
    HidReport lastReport_{};
};

}  // namespace pd
