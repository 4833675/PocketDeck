#pragma once

#include "core/ssh_hosts.h"

namespace pd {

struct SshHostLoadResult {
    SshHosts hosts{};
    bool storageReady = false;
    bool found = false;
    bool valid = true;
};

class SshHostStore {
public:
    SshHostLoadResult load() const;
    bool save(const SshHosts& hosts) const;
    bool clear() const;
};

}  // namespace pd
