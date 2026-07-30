#pragma once

#include "apps/media/media_app_model.h"
#include "core/app.h"

namespace pd {

class MediaApp final : public App {
public:
    AppId id() const override { return AppId::Media; }
    const char* title() const override { return "MEDIA"; }
    InputMode inputMode() const override { return InputMode::Text; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    void scan(SystemContext& context);

    MediaAppModel model_{};
};

}  // namespace pd
