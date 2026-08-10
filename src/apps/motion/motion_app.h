#pragma once

#include "apps/motion/motion_app_model.h"
#include "core/app.h"

namespace pd {

class MotionApp final : public App {
public:
    AppId id() const override { return AppId::Motion; }
    const char* title() const override { return "MOTION"; }
    InputMode inputMode() const override { return InputMode::System; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    MotionAppModel model_;
};

}  // namespace pd
