#pragma once

#include <array>
#include <cstdint>

namespace pd {

struct SystemSettings {
    static constexpr uint16_t kVersion = 1;
    static constexpr std::size_t kNameCapacity = 25;

    uint16_t version = kVersion;
    uint8_t brightness = 78;
    uint8_t volume = 55;
    uint16_t sleepSeconds = 120;
    bool keyClick = true;
    bool wifiEnabled = false;
    bool bleEnabled = true;
    std::array<char, kNameCapacity> deviceName{};
    std::array<char, kNameCapacity> hostLabel{};

    static SystemSettings defaults();
};

SystemSettings sanitizeSettings(SystemSettings settings);

}  // namespace pd

