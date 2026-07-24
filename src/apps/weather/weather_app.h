#pragma once

#include "core/app.h"

namespace pd {

class WeatherApp final : public App {
public:
    AppId id() const override { return AppId::Weather; }
    const char* title() const override { return "WEATHER"; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    bool refreshPending_ = true;
};

}  // namespace pd
