#pragma once

#include <array>
#include <cstdint>

#include "core/ssh_hosts.h"

namespace pd {

inline constexpr uint32_t kSshHostRecordMagic = 0x50445348u;
inline constexpr uint16_t kSshHostRecordVersion = 1;

#pragma pack(push, 1)
struct StoredSshHost {
    std::array<char, kSshLabelCapacity> label{};
    std::array<char, kSshHostnameCapacity> hostname{};
    std::array<char, kSshUsernameCapacity> username{};
    uint16_t port = 22;
};

struct SshHostRecord {
    uint32_t magic = kSshHostRecordMagic;
    uint16_t version = kSshHostRecordVersion;
    uint8_t count = 0;
    uint8_t reserved = 0;
    std::array<StoredSshHost, kSshHostCapacity> hosts{};
    uint32_t checksum = 0;
};
#pragma pack(pop)

SshHostRecord encodeSshHosts(const SshHosts& hosts);
bool decodeSshHosts(const SshHostRecord& record, SshHosts& hosts);

}  // namespace pd
