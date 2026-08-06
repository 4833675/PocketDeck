#pragma once

#include "core/localization.h"
#include "services/ble_keyboard_service.h"

namespace pd {

const char* localizedKeyboardStateLabel(BleKeyboardState state,
                                        UiLanguage language);
const char* localizedKeyboardErrorLabel(BleKeyboardError error,
                                        UiLanguage language);

}  // namespace pd
