#pragma once

#include "core/system_settings.h"

namespace pd {

struct SettingsLoadResult {
    SystemSettings settings = SystemSettings::defaults();
    bool storageReady = false;
    bool found = false;
    bool valid = true;
};

class SettingsStore {
public:
    SettingsLoadResult load() const;
    bool save(const SystemSettings& settings) const;
    bool clear() const;
};

}  // namespace pd
