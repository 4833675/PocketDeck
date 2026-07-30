#include "services/ssh_host_store.h"

#include <Preferences.h>

#include "core/ssh_host_record.h"

namespace pd {
namespace {

constexpr char kNamespace[] = "pocketssh";
constexpr char kHostsKey[] = "hosts";

}  // namespace

SshHostLoadResult SshHostStore::load() const {
    SshHostLoadResult result;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        result.valid = false;
        return result;
    }
    result.storageReady = true;
    const std::size_t length = preferences.getBytesLength(kHostsKey);
    if (length == 0) {
        preferences.end();
        return result;
    }
    result.found = true;
    if (length != sizeof(SshHostRecord)) {
        result.valid = false;
        preferences.end();
        return result;
    }
    SshHostRecord record;
    const std::size_t read = preferences.getBytes(kHostsKey, &record, sizeof(record));
    preferences.end();
    if (read != sizeof(record) || !decodeSshHosts(record, result.hosts)) {
        result.valid = false;
    }
    return result;
}

bool SshHostStore::save(const SshHosts& hosts) const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const SshHostRecord record = encodeSshHosts(hosts);
    const bool saved = preferences.putBytes(kHostsKey, &record, sizeof(record)) == sizeof(record);
    preferences.end();
    return saved;
}

bool SshHostStore::clear() const {
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool cleared = preferences.clear();
    preferences.end();
    return cleared;
}

}  // namespace pd
