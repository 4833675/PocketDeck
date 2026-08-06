#pragma once

#include <cstdint>

namespace pd {

enum class WifiRecoveryAction : uint8_t {
    None,
    ReconnectLast,
    ScanProfiles,
};

// Pure timing policy for Wi-Fi recovery. The driver/service owns the actual
// reconnect and scan operations; this class only guarantees that a transient
// link loss is given a direct-reconnect chance before expensive full scans.
class WifiRecoveryPolicy {
public:
    void reset();
    void noteConnected();
    void noteLinkLost(uint32_t nowMs);
    void noteAttemptFailed(uint32_t nowMs);
    void noteScanFailed(uint32_t nowMs);
    WifiRecoveryAction takeDueAction(uint32_t nowMs);

private:
    void schedule(WifiRecoveryAction action, uint32_t nowMs, uint32_t delayMs);
    uint32_t nextBackoffMs();

    WifiRecoveryAction pending_ = WifiRecoveryAction::None;
    uint32_t dueAtMs_ = 0;
    uint8_t backoffIndex_ = 0;
};

const char* wifiRecoveryActionLabel(WifiRecoveryAction action);

}  // namespace pd
