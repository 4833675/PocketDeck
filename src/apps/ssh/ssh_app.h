#pragma once

#include "apps/ssh/ssh_app_model.h"
#include "core/app.h"
#include "core/ssh_hosts.h"
#include "core/ssh_retry_policy.h"
#include "core/terminal_buffer.h"

namespace pd {

class SshApp final : public App {
public:
    AppId id() const override { return AppId::Ssh; }
    const char* title() const override { return "SSH"; }
    InputMode inputMode() const override { return model_.inputMode(); }
    void onEnter(SystemContext& context) override;
    void onExit(SystemContext& context) override;
    void onInput(const InputEvent& event, SystemContext& context) override;
    void update(uint32_t nowMs, SystemContext& context) override;
    void render(Display& display, const SystemContext& context) override;

private:
    void handleResult(const SshResult& result, SystemContext& context);
    void connectHost(std::size_t index, SystemContext& context);
    void reconnect(SystemContext& context);
    bool saveHosts(SystemContext& context);

    SshAppModel model_;
    SshHosts hosts_;
    TerminalBuffer terminal_;
    SshHost activeHost_{};
    bool hostsLoaded_ = false;
    bool serviceReady_ = false;
    bool hasActiveHost_ = false;
    bool storeError_ = false;
    uint32_t lastSshGeneration_ = 0;
    SshRetryPolicy retryPolicy_;
};

}  // namespace pd
