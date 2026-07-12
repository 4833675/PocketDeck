#pragma once

#include "core/app.h"

namespace pd {

class KeyboardApp final : public App {
public:
    AppId id() const override { return AppId::Keyboard; }
    const char* title() const override { return "KEYBOARD"; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;
};

}  // namespace pd

