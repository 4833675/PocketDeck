#pragma once

#include <array>
#include <cstdint>

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
    static constexpr std::size_t kSsidCapacity = 33;

    std::array<char, kSsidCapacity> ssid{};
    int32_t rssi = -127;
    uint8_t channel = 0;
    bool secured = true;
};

struct WifiSnapshot {
    static constexpr std::size_t kNetworkCapacity = 8;

    WifiState state = WifiState::Disabled;
    bool enabled = false;
    bool connected = false;
    bool hasSavedNetwork = false;
    bool ntpSynced = false;
    std::array<char, WifiNetwork::kSsidCapacity> ssid{};
    std::array<char, 16> ip{};
    std::array<char, 16> gateway{};
    std::array<char, 16> dns{};
    int32_t rssi = -127;
    int16_t lastStatus = 0;
    uint32_t connectedForMs = 0;
    int64_t utcEpoch = 0;
    std::array<WifiNetwork, kNetworkCapacity> networks{};
    uint8_t networkCount = 0;
};

const char* wifiStateLabel(WifiState state);

}  // namespace pd
