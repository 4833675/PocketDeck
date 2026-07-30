#include "core/ssh_host_record.h"

#include <cstddef>
#include <type_traits>

namespace pd {
namespace {

static_assert(std::is_trivially_copyable<SshHostRecord>::value,
              "SSH host record must remain byte-serializable");

uint32_t recordChecksum(const SshHostRecord& record) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint32_t hash = 2166136261u;
    for (std::size_t index = 0; index < offsetof(SshHostRecord, checksum); ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace

SshHostRecord encodeSshHosts(const SshHosts& hosts) {
    SshHostRecord record;
    record.count = static_cast<uint8_t>(hosts.size());
    for (std::size_t index = 0; index < hosts.size(); ++index) {
        record.hosts[index].label = hosts.at(index).label;
        record.hosts[index].hostname = hosts.at(index).hostname;
        record.hosts[index].username = hosts.at(index).username;
        record.hosts[index].port = hosts.at(index).port;
    }
    record.checksum = recordChecksum(record);
    return record;
}

bool decodeSshHosts(const SshHostRecord& record, SshHosts& hosts) {
    if (record.magic != kSshHostRecordMagic || record.version != kSshHostRecordVersion ||
        record.reserved != 0 || record.count > kSshHostCapacity ||
        record.checksum != recordChecksum(record)) {
        return false;
    }

    SshHosts decoded;
    for (std::size_t index = record.count; index > 0; --index) {
        const StoredSshHost& stored = record.hosts[index - 1];
        if (!decoded.upsert(stored.label.data(), stored.hostname.data(),
                            stored.username.data(), stored.port)) {
            return false;
        }
    }
    hosts = decoded;
    return true;
}

}  // namespace pd
