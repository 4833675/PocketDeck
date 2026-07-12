#pragma once

#include "core/hid_report.h"

namespace pd {

struct ReportDecision {
    bool send = false;
    HidReport report{};
};

class BleKeyboardPolicy {
public:
    static bool peerAllowed(bool hasStoredBond, bool incomingPeerIsBonded) {
        return !hasStoredBond || incomingPeerIsBonded;
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

