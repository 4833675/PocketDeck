#pragma once

#include "apps/lora/lora_app_model.h"
#include "core/app.h"

namespace pd {

class LoRaApp final : public App {
public:
    AppId id() const override { return AppId::LoRa; }
    const char* title() const override { return "LORA"; }
    InputMode inputMode() const override { return InputMode::Text; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    LoRaAppModel model_;
};

}  // namespace pd
