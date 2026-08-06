#pragma once

#include "core/localization.h"

namespace pd {

const char* localizedResetReasonLabel(const char* reason,
                                      UiLanguage language);
const char* localizedStorageErrorLabel(const char* error,
                                       UiLanguage language);

}  // namespace pd
