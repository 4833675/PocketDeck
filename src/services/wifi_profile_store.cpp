#include "services/wifi_profile_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <Preferences.h>

namespace pd {
namespace {

constexpr char kNamespace[] = "pocketwifi";
constexpr char kProfilesKey[] = "profiles";
constexpr uint32_t kRecordMagic = 0x50445746u;
constexpr uint16_t kRecordVersion = 1;

#pragma pack(push, 1)
struct StoredWifiProfileV1 {
    std::array<char, kWifiSsidCapacity> ssid{};
    std::array<char, kWifiPasswordCapacity> password{};
};

struct StoredWifiProfilesV1 {
    uint32_t magic = kRecordMagic;
    uint16_t version = kRecordVersion;
    uint8_t count = 0;
    uint8_t reserved = 0;
    std::array<StoredWifiProfileV1, kWifiProfileCapacity> profiles{};
    uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable<StoredWifiProfilesV1>::value,
              "Wi-Fi profile record must remain byte-serializable");

uint32_t checksum(const StoredWifiProfilesV1& record) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint32_t hash = 2166136261u;
    for (std::size_t index = 0; index < offsetof(StoredWifiProfilesV1, checksum); ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

bool terminated(const char* value, std::size_t capacity) {
    return std::memchr(value, '\0', capacity) != nullptr;
}

StoredWifiProfilesV1 encode(const WifiProfiles& profiles) {
    StoredWifiProfilesV1 record;
    record.count = static_cast<uint8_t>(profiles.size());
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        record.profiles[index].ssid = profiles.at(index).ssid;
        record.profiles[index].password = profiles.at(index).password;
    }
    record.checksum = checksum(record);
    return record;
}

bool decode(const StoredWifiProfilesV1& record, WifiProfiles& profiles) {
    if (record.magic != kRecordMagic || record.version != kRecordVersion ||
        record.reserved != 0 || record.count > kWifiProfileCapacity ||
        record.checksum != checksum(record)) {
        return false;
    }

    WifiProfiles decoded;
    for (int index = static_cast<int>(record.count) - 1; index >= 0; --index) {
        const auto& stored = record.profiles[static_cast<std::size_t>(index)];
        if (!terminated(stored.ssid.data(), stored.ssid.size()) ||
            !terminated(stored.password.data(), stored.password.size()) ||
            stored.ssid[0] == '\0' || decoded.find(stored.ssid.data()) != nullptr ||
            !decoded.upsert(stored.ssid.data(), stored.password.data())) {
            return false;
        }
    }
    profiles = decoded;
    return true;
}

}  // namespace

WifiProfilesLoadResult WifiProfileStore::load() const {
    WifiProfilesLoadResult result;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        result.valid = false;
        return result;
    }

    result.storageReady = true;
    const std::size_t length = preferences.getBytesLength(kProfilesKey);
    if (length == 0) {
        preferences.end();
        return result;
    }

    result.found = true;
    if (length != sizeof(StoredWifiProfilesV1)) {
        result.valid = false;
        preferences.end();
        return result;
    }

    StoredWifiProfilesV1 record;
    const std::size_t read = preferences.getBytes(kProfilesKey, &record, sizeof(record));
    preferences.end();
    if (read != sizeof(record) || !decode(record, result.profiles)) result.valid = false;
    return result;
}

bool WifiProfileStore::save(const WifiProfiles& profiles) const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const StoredWifiProfilesV1 record = encode(profiles);
    const bool saved =
        preferences.putBytes(kProfilesKey, &record, sizeof(record)) == sizeof(record);
    preferences.end();
    return saved;
}

bool WifiProfileStore::clear() const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool cleared = preferences.clear();
    preferences.end();
    return cleared;
}

}  // namespace pd
