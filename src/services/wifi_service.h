#pragma once

#include <cstdint>

#include "core/wifi_data.h"

namespace pd {

class WifiService {
public:
    bool begin(bool enabled);
    void update(uint32_t nowMs);
    void setEnabled(bool enabled);
    bool startScan();
    bool connect(const char* ssid, const char* password);
    bool forgetNetwork();

    const WifiSnapshot& snapshot() const { return snapshot_; }

private:
    static constexpr uint32_t kConnectTimeoutMs = 20000;

    bool savedNetworkAvailable() const;
    void loadSavedSsid();
    void collectScanResults(int16_t count);
    void refreshLinkDetails(uint32_t nowMs);
    void setState(WifiState state, uint32_t nowMs);
    void requestTimeSync();
    void clearLinkDetails();

    WifiSnapshot snapshot_{};
    uint32_t stateSinceMs_ = 0;
    uint32_t connectedAtMs_ = 0;
    bool timeRequested_ = false;
    bool wasConnected_ = false;
};

}  // namespace pd
