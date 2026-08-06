#include "services/settings_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <Preferences.h>

namespace pd {
namespace {

constexpr char kNamespace[] = "pocketdeck";
constexpr char kSettingsKey[] = "settings";
constexpr uint32_t kRecordMagic = 0x50444543u;

#pragma pack(push, 1)
struct StoredSettingsV1 {
    uint32_t magic = kRecordMagic;
    uint16_t version = SystemSettings::kVersion;
    uint8_t brightness = 0;
    uint8_t volume = 0;
    uint16_t sleepSeconds = 0;
    uint8_t flags = 0;
    std::array<char, SystemSettings::kNameCapacity> deviceName{};
    std::array<char, SystemSettings::kNameCapacity> hostLabel{};
    uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable<StoredSettingsV1>::value,
              "settings record must remain byte-serializable");

uint32_t checksum(const StoredSettingsV1& record) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint32_t hash = 2166136261u;
    for (std::size_t index = 0; index < offsetof(StoredSettingsV1, checksum); ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

StoredSettingsV1 encode(const SystemSettings& input) {
    const SystemSettings settings = sanitizeSettings(input);
    StoredSettingsV1 record;
    record.brightness = settings.brightness;
    record.volume = settings.volume;
    record.sleepSeconds = settings.sleepSeconds;
    record.flags = static_cast<uint8_t>((settings.keyClick ? 0x01u : 0u) |
                                        (settings.wifiEnabled ? 0x02u : 0u) |
                                        (settings.bleEnabled ? 0x04u : 0u) |
                                        (isSimplifiedChinese(settings.language) ? 0x08u
                                                                                 : 0u));
    record.deviceName = settings.deviceName;
    record.hostLabel = settings.hostLabel;
    record.checksum = checksum(record);
    return record;
}

bool decode(const StoredSettingsV1& record, SystemSettings& settings) {
    if (record.magic != kRecordMagic || record.version != SystemSettings::kVersion ||
        record.checksum != checksum(record) || (record.flags & 0xF0u) != 0) {
        return false;
    }

    SystemSettings decoded = SystemSettings::defaults();
    decoded.brightness = record.brightness;
    decoded.volume = record.volume;
    decoded.sleepSeconds = record.sleepSeconds;
    decoded.keyClick = (record.flags & 0x01u) != 0;
    decoded.wifiEnabled = (record.flags & 0x02u) != 0;
    decoded.bleEnabled = (record.flags & 0x04u) != 0;
    decoded.language = (record.flags & 0x08u) != 0
                           ? UiLanguage::SimplifiedChinese
                           : UiLanguage::English;
    decoded.deviceName = record.deviceName;
    decoded.hostLabel = record.hostLabel;
    const SystemSettings sanitized = sanitizeSettings(decoded);
    if (decoded.brightness != sanitized.brightness || decoded.volume != sanitized.volume ||
        decoded.sleepSeconds != sanitized.sleepSeconds ||
        decoded.language != sanitized.language ||
        decoded.deviceName != sanitized.deviceName || decoded.hostLabel != sanitized.hostLabel) {
        return false;
    }
    settings = sanitized;
    return true;
}

}  // namespace

SettingsLoadResult SettingsStore::load() const {
    SettingsLoadResult result;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        result.valid = false;
        return result;
    }

    result.storageReady = true;
    const std::size_t length = preferences.getBytesLength(kSettingsKey);
    if (length == 0) {
        preferences.end();
        return result;
    }

    result.found = true;
    if (length != sizeof(StoredSettingsV1)) {
        result.valid = false;
        preferences.end();
        return result;
    }

    StoredSettingsV1 record;
    const std::size_t read = preferences.getBytes(kSettingsKey, &record, sizeof(record));
    preferences.end();
    if (read != sizeof(record) || !decode(record, result.settings)) result.valid = false;
    return result;
}

bool SettingsStore::save(const SystemSettings& settings) const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const StoredSettingsV1 record = encode(settings);
    const bool saved = preferences.putBytes(kSettingsKey, &record, sizeof(record)) == sizeof(record);
    preferences.end();
    return saved;
}

bool SettingsStore::clear() const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool cleared = preferences.clear();
    preferences.end();
    return cleared;
}

}  // namespace pd
