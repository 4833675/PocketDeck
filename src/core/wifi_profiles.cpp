#include "core/wifi_profiles.h"

#include <cstring>

namespace pd {
namespace {

bool validText(const char* value, std::size_t capacity, bool allowEmpty) {
    if (value == nullptr) return false;
    const void* terminator = std::memchr(value, '\0', capacity);
    if (terminator == nullptr) return false;
    return allowEmpty || value[0] != '\0';
}

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const char* source) {
    destination.fill('\0');
    if (source != nullptr) std::strncpy(destination.data(), source, Size - 1);
}

void wipe(WifiProfile& profile) {
    profile.ssid.fill('\0');
    profile.password.fill('\0');
}

}  // namespace

bool WifiProfiles::upsert(const char* ssid, const char* password) {
    if (!validText(ssid, kWifiSsidCapacity, false) ||
        !validText(password, kWifiPasswordCapacity, true)) {
        return false;
    }

    WifiProfile updated;
    copyText(updated.ssid, ssid);
    copyText(updated.password, password);

    const int existing = findIndex(ssid);
    std::size_t source = existing >= 0 ? static_cast<std::size_t>(existing) : count_;
    if (existing < 0 && count_ < entries_.size()) ++count_;
    if (source >= entries_.size()) source = entries_.size() - 1;

    for (std::size_t index = source; index > 0; --index) {
        entries_[index] = entries_[index - 1];
    }
    entries_[0] = updated;
    return true;
}

bool WifiProfiles::touch(const char* ssid) {
    const int existing = findIndex(ssid);
    if (existing < 0) return false;
    const std::size_t source = static_cast<std::size_t>(existing);
    if (source == 0) return true;

    const WifiProfile selected = entries_[source];
    for (std::size_t index = source; index > 0; --index) {
        entries_[index] = entries_[index - 1];
    }
    entries_[0] = selected;
    return true;
}

bool WifiProfiles::erase(const char* ssid) {
    const int existing = findIndex(ssid);
    if (existing < 0) return false;

    const std::size_t removed = static_cast<std::size_t>(existing);
    wipe(entries_[removed]);
    for (std::size_t index = removed; index + 1 < count_; ++index) {
        entries_[index] = entries_[index + 1];
    }
    if (count_ > 0) {
        --count_;
        wipe(entries_[count_]);
    }
    return true;
}

void WifiProfiles::clear() {
    for (auto& entry : entries_) wipe(entry);
    count_ = 0;
}

int WifiProfiles::findIndex(const char* ssid) const {
    if (ssid == nullptr || ssid[0] == '\0') return -1;
    for (std::size_t index = 0; index < count_; ++index) {
        if (std::strcmp(entries_[index].ssid.data(), ssid) == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

const WifiProfile* WifiProfiles::find(const char* ssid) const {
    const int index = findIndex(ssid);
    return index >= 0 ? &entries_[static_cast<std::size_t>(index)] : nullptr;
}

}  // namespace pd
