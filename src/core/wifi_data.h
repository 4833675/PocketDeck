#pragma once

#include <array>
#include <cstdint>

#include "core/wifi_recovery_policy.h"
#include "core/wifi_profiles.h"

namespace pd {

enum class WifiState : uint8_t {
    Disabled,
    Idle,
    Scanning,
    Connecting,
    Connected,
    Error,
};

struct WifiNetwork {
    std::array<char, kWifiSsidCapacity> ssid{};
    int32_t rssi = -127;
    uint8_t channel = 0;
    bool secured = true;
    bool saved = false;
};

struct WifiSavedNetwork {
    std::array<char, kWifiSsidCapacity> ssid{};
};

struct WifiSnapshot {
    static constexpr std::size_t kNetworkCapacity = kWifiProfileCapacity;

    WifiState state = WifiState::Disabled;
    bool enabled = false;
    bool connected = false;
    bool hasSavedNetwork = false;
    bool ntpSynced = false;
    std::array<char, kWifiSsidCapacity> ssid{};
    std::array<char, 16> ip{};
    std::array<char, 16> gateway{};
    std::array<char, 16> dns{};
    int32_t rssi = -127;
    int16_t lastStatus = 0;
    int16_t lastScanResult = 0;
    uint8_t lastDisconnectReason = 0;
    uint32_t connectedForMs = 0;
    uint32_t scanGeneration = 0;
    uint32_t disconnectGeneration = 0;
    uint32_t lostIpGeneration = 0;
    WifiRecoveryAction lastRecoveryAction = WifiRecoveryAction::None;
    uint32_t recoveryGeneration = 0;
    int64_t utcEpoch = 0;
    std::array<WifiNetwork, kNetworkCapacity> networks{};
    uint8_t networkCount = 0;
    std::array<WifiSavedNetwork, kWifiProfileCapacity> savedNetworks{};
    uint8_t savedNetworkCount = 0;
    uint8_t autoCandidateCount = 0;
};

const char* wifiStateLabel(WifiState state);
const char* wifiDisconnectReasonLabel(uint8_t reason);

}  // namespace pd
