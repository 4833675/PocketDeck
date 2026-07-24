#pragma once

#include "core/app.h"

namespace pd {

class GpsApp final : public App {
public:
    AppId id() const override { return AppId::Gps; }
    const char* title() const override { return "GPS"; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    uint8_t page_ = 0;
};

}  // namespace pd
