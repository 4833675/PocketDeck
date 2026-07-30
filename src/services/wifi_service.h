#pragma once

#include <array>
#include <cstdint>

#include "core/wifi_data.h"
#include "core/wifi_profiles.h"
#include "services/wifi_profile_store.h"

namespace pd {

class WifiService {
public:
    bool begin(bool enabled);
    void update(uint32_t nowMs);
    void setEnabled(bool enabled);
    bool startScan();
    bool connect(const char* ssid, const char* password);
    bool forgetNetwork(const char* ssid = nullptr);

    const WifiSnapshot& snapshot() const { return snapshot_; }

private:
    enum class ScanPurpose : uint8_t {
        None,
        User,
        AutoConnect,
    };

    static constexpr uint32_t kConnectTimeoutMs = 20000;
    static constexpr uint32_t kScanStartTimeoutMs = 4000;
    static constexpr uint32_t kScanCompleteTimeoutMs = 15000;
    static constexpr uint32_t kScanRetryMs = 250;
    static constexpr uint32_t kDisconnectedScanIntervalMs = 15000;

    void importLegacyProfile();
    bool persistProfiles();
    void syncSavedNetworks();
    void requestScan(ScanPurpose purpose, uint32_t nowMs);
    void processScan(uint32_t nowMs);
    void completeScan(int16_t count, uint32_t nowMs);
    void failScan(uint32_t nowMs);
    void collectScanResults(int16_t count);
    void buildCandidates(int16_t count);
    bool beginNextCandidate(uint32_t nowMs);
    bool beginConnection(const char* ssid, const char* password, bool saveOnSuccess,
                         bool candidateConnection, uint32_t nowMs);
    void completeConnection(uint32_t nowMs);
    void failConnection(uint32_t nowMs);
    void clearPendingCredentials();
    void scheduleAutoScan(uint32_t nowMs, uint32_t delayMs);
    void refreshLinkDetails(uint32_t nowMs);
    void setState(WifiState state, uint32_t nowMs);
    void requestTimeSync();
    void clearLinkDetails();

    WifiSnapshot snapshot_{};
    WifiProfileStore profileStore_;
    WifiProfiles profiles_;
    ScanPurpose scanPurpose_ = ScanPurpose::None;
    bool scanStartPending_ = false;
    bool scanActive_ = false;
    uint32_t scanRequestedAtMs_ = 0;
    uint32_t scanRetryAtMs_ = 0;
    uint32_t nextAutoScanAtMs_ = 0;
    std::array<uint8_t, kWifiProfileCapacity> candidates_{};
    uint8_t candidateCount_ = 0;
    uint8_t candidatePosition_ = 0;
    bool connectionActive_ = false;
    bool saveOnConnect_ = false;
    bool candidateConnection_ = false;
    std::array<char, kWifiSsidCapacity> pendingSsid_{};
    std::array<char, kWifiPasswordCapacity> pendingPassword_{};
    uint32_t stateSinceMs_ = 0;
    uint32_t connectedAtMs_ = 0;
    bool timeRequested_ = false;
    bool wasConnected_ = false;
};

}  // namespace pd
