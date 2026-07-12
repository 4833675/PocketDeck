#include "core/system_settings.h"

#include <algorithm>
#include <cstring>

namespace pd {
namespace {

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const char* source) {
    destination.fill('\0');
    std::strncpy(destination.data(), source, Size - 1);
}

template <std::size_t Size>
bool validText(const std::array<char, Size>& value) {
    if (value[0] == '\0') return false;
    for (const char character : value) {
        if (character == '\0') return true;
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20 || byte > 0x7E) return false;
    }
    return false;
}

}  // namespace

SystemSettings SystemSettings::defaults() {
    SystemSettings settings;
    copyText(settings.deviceName, "Pocket Deck");
    copyText(settings.hostLabel, "Mac");
    return settings;
}

SystemSettings sanitizeSettings(SystemSettings settings) {
    if (settings.version != SystemSettings::kVersion) return SystemSettings::defaults();

    settings.brightness = std::min<uint8_t>(settings.brightness, 100);
    settings.volume = std::min<uint8_t>(settings.volume, 100);
    if (settings.sleepSeconds < 15) settings.sleepSeconds = 15;
    if (settings.sleepSeconds > 3600) settings.sleepSeconds = 3600;
    if (!validText(settings.deviceName)) copyText(settings.deviceName, "Pocket Deck");
    if (!validText(settings.hostLabel)) copyText(settings.hostLabel, "Mac");
    return settings;
}

}  // namespace pd
