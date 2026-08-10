#pragma once

#include "apps/remote/remote_app_model.h"
#include "core/app.h"

namespace pd {

enum class RemoteLocalState : uint8_t {
    None,
    Sent,
    Error,
};

class RemoteApp final : public App {
public:
    AppId id() const override { return AppId::Remote; }
    const char* title() const override { return "REMOTE"; }
    InputMode inputMode() const override { return InputMode::Text; }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    RemoteAppModel model_;
    SonyIrCommand lastCommand_ = SonyIrCommand::None;
    RemoteLocalState localState_ = RemoteLocalState::None;
};

}  // namespace pd
