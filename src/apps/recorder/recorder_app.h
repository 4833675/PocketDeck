#pragma once

#include <cstdint>

#include "apps/recorder/recorder_app_model.h"
#include "core/app.h"

namespace pd {

class RecorderApp final : public App {
public:
    AppId id() const override { return AppId::Recorder; }
    const char* title() const override { return "RECORDER"; }
    InputMode inputMode() const override { return model_.inputMode(); }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    bool storageMounted(const SystemContext& context) const;
    void logStateChange(SystemContext& context);

    RecorderAppModel model_{};
    bool loggedSnapshotValid_ = false;
    uint8_t loggedState_ = 0;
    uint8_t loggedError_ = 0;
};

}  // namespace pd
