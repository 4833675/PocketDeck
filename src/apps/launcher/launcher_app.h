#pragma once

#include "apps/launcher/launcher_model.h"
#include "core/app.h"

namespace pd {

class LauncherApp final : public App {
public:
    AppId id() const override { return AppId::Launcher; }
    const char* title() const override { return "POCKET DECK"; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    LauncherModel model_;
};

}  // namespace pd

