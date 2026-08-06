#include "core/wifi_recovery_policy.h"

#include <array>

namespace pd {
namespace {

constexpr std::array<uint32_t, 4> kRecoveryBackoffMs{
    2000,
    5000,
    15000,
    30000,
};

bool deadlineReached(uint32_t nowMs, uint32_t dueAtMs) {
    return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}

}  // namespace

void WifiRecoveryPolicy::reset() {
    pending_ = WifiRecoveryAction::None;
    dueAtMs_ = 0;
    backoffIndex_ = 0;
}

void WifiRecoveryPolicy::noteConnected() {
    reset();
}

void WifiRecoveryPolicy::noteLinkLost(uint32_t nowMs) {
    backoffIndex_ = 1;
    schedule(WifiRecoveryAction::ReconnectLast, nowMs, kRecoveryBackoffMs.front());
}

void WifiRecoveryPolicy::noteAttemptFailed(uint32_t nowMs) {
    schedule(WifiRecoveryAction::ScanProfiles, nowMs, nextBackoffMs());
}

void WifiRecoveryPolicy::noteScanFailed(uint32_t nowMs) {
    schedule(WifiRecoveryAction::ScanProfiles, nowMs, nextBackoffMs());
}

WifiRecoveryAction WifiRecoveryPolicy::takeDueAction(uint32_t nowMs) {
    if (pending_ == WifiRecoveryAction::None || !deadlineReached(nowMs, dueAtMs_)) {
        return WifiRecoveryAction::None;
    }
    const WifiRecoveryAction action = pending_;
    pending_ = WifiRecoveryAction::None;
    dueAtMs_ = 0;
    return action;
}

void WifiRecoveryPolicy::schedule(WifiRecoveryAction action, uint32_t nowMs,
                                  uint32_t delayMs) {
    pending_ = action;
    dueAtMs_ = nowMs + delayMs;
}

uint32_t WifiRecoveryPolicy::nextBackoffMs() {
    const uint8_t index = backoffIndex_ < kRecoveryBackoffMs.size()
                              ? backoffIndex_
                              : static_cast<uint8_t>(kRecoveryBackoffMs.size() - 1);
    if (backoffIndex_ + 1 < kRecoveryBackoffMs.size()) ++backoffIndex_;
    return kRecoveryBackoffMs[index];
}

const char* wifiRecoveryActionLabel(WifiRecoveryAction action) {
    switch (action) {
        case WifiRecoveryAction::None: return "NONE";
        case WifiRecoveryAction::ReconnectLast: return "DIRECT_RECONNECT";
        case WifiRecoveryAction::ScanProfiles: return "SCAN_FALLBACK";
    }
    return "UNKNOWN";
}

}  // namespace pd
