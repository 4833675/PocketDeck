#pragma once

#include <array>
#include <cstdint>

#include "core/app_id.h"
#include "core/input.h"

namespace pd {

class LauncherModel {
public:
    AppId selected() const { return kApps[index_]; }
    uint8_t selectedIndex() const { return index_; }
    AppId handle(InputAction action);

private:
    static constexpr std::array<AppId, 10> kApps{
        AppId::Keyboard, AppId::Ssh,     AppId::Gps,
        AppId::Motion,   AppId::Remote,  AppId::LoRa,
        AppId::Media,    AppId::Recorder, AppId::Weather, AppId::Settings,
    };
    uint8_t index_ = 0;
};

}  // namespace pd
