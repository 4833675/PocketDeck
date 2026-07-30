#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kWifiSsidCapacity = 33;
inline constexpr std::size_t kWifiPasswordCapacity = 65;
inline constexpr std::size_t kWifiProfileCapacity = 8;

struct WifiProfile {
    std::array<char, kWifiSsidCapacity> ssid{};
    std::array<char, kWifiPasswordCapacity> password{};
};

class WifiProfiles {
public:
    bool upsert(const char* ssid, const char* password);
    bool touch(const char* ssid);
    bool erase(const char* ssid);
    void clear();

    int findIndex(const char* ssid) const;
    const WifiProfile* find(const char* ssid) const;
    const WifiProfile& at(std::size_t index) const { return entries_[index]; }
    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

private:
    std::array<WifiProfile, kWifiProfileCapacity> entries_{};
    std::size_t count_ = 0;
};

}  // namespace pd
