#pragma once

#include "core/wifi_profiles.h"

namespace pd {

struct WifiProfilesLoadResult {
    WifiProfiles profiles;
    bool storageReady = false;
    bool found = false;
    bool valid = true;
};

class WifiProfileStore {
public:
    WifiProfilesLoadResult load() const;
    bool save(const WifiProfiles& profiles) const;
    bool clear() const;
};

}  // namespace pd
