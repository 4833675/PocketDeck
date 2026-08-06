#pragma once

#include "core/gps_data.h"
#include "core/localization.h"

namespace pd {

const char* localizedGpsStateLabel(GpsState state, UiLanguage language);
const char* localizedGpsCompassPoint(double courseDegrees, UiLanguage language);
const char* localizedGpsFixQualityLabel(char quality, UiLanguage language);
const char* localizedGpsFixModeLabel(char mode, UiLanguage language);

}  // namespace pd
