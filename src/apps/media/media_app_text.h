#pragma once

#include "core/localization.h"
#include "services/media_service.h"

namespace pd {

const char* localizedMediaStateLabel(MediaPlaybackState state,
                                     UiLanguage language);
const char* localizedMediaDetailLabel(const char* detail,
                                      UiLanguage language);

}  // namespace pd
