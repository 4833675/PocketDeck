#pragma once

#include "core/localization.h"
#include "core/ssh_data.h"

namespace pd {

const char* localizedSshStateLabel(SshState state, UiLanguage language);
const char* localizedSshErrorLabel(SshError error, UiLanguage language);

}  // namespace pd
