#pragma once

#include <cstdint>

#include "core/localization.h"

namespace pd {

// These numeric values mirror the portable RecorderState/RecorderError order.
// Keeping this text unit independent of Arduino FS lets native tests cover every
// visible bilingual label without pulling in hardware service headers.
const char* localizedRecorderStateLabel(uint8_t state, UiLanguage language);
const char* localizedRecorderErrorLabel(uint8_t error, UiLanguage language);
const char* localizedRecorderLauncherTitle(UiLanguage language);
const char* localizedRecorderLauncherSubtitle(UiLanguage language);

}  // namespace pd
