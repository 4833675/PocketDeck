#pragma once

#include <array>

#include "apps/settings/settings_model.h"
#include "core/app.h"

namespace pd {

class SettingsApp final : public App {
public:
    AppId id() const override { return AppId::Settings; }
    const char* title() const override { return "SETTINGS"; }
    InputMode inputMode() const override {
        return model_.page() == SettingsPage::WifiPassword ? InputMode::Text
                                                           : InputMode::System;
    }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    void clearWifiEntry();

    SettingsModel model_;
    std::array<char, 33> selectedSsid_{};
    std::array<char, 65> wifiPassword_{};
};

}  // namespace pd
