#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/input.h"
#include "core/ssh_hosts.h"

namespace pd {

enum class SshPage : uint8_t {
    HostList,
    Editor,
    ConfirmDelete,
    Connecting,
    Terminal,
    QuickCommands,
    Disconnected,
};

enum class SshEffect : uint8_t {
    None,
    GoHome,
    EditSelected,
    ConnectSelected,
    SaveEditor,
    DeleteSelected,
    CancelConnection,
    Reconnect,
    SendQuickCommand,
};

struct SshResult {
    SshEffect effect = SshEffect::None;
    std::size_t index = static_cast<std::size_t>(-1);
};

class SshAppModel {
public:
    static constexpr std::size_t kNewHostIndex = static_cast<std::size_t>(-1);

    void reset();
    SshResult handle(const InputEvent& event, std::size_t hostCount);
    void beginEdit(const SshHost& host, std::size_t index);
    void showConnecting() { page_ = SshPage::Connecting; }
    void showTerminal() { page_ = SshPage::Terminal; }
    void showDisconnected() { page_ = SshPage::Disconnected; }
    void showHostList() { page_ = SshPage::HostList; }

    SshPage page() const { return page_; }
    InputMode inputMode() const;
    std::size_t selectedHost() const { return selectedHost_; }
    std::size_t selectedField() const { return selectedField_; }
    std::size_t editingIndex() const { return editingIndex_; }
    bool editorHasError() const { return editorHasError_; }
    const char* editorValue(std::size_t field) const;
    bool editedHost(SshHost& host) const;
    const char* quickCommand() const;
    std::size_t selectedQuickCommand() const { return selectedQuickCommand_; }

private:
    void moveSelection(int direction, std::size_t count);
    void startNewEditor();
    void moveField(int direction);
    void appendEditorCharacter(char character);
    void eraseEditorCharacter();

    SshPage page_ = SshPage::HostList;
    std::size_t selectedHost_ = 0;
    std::size_t selectedField_ = 0;
    std::size_t editingIndex_ = kNewHostIndex;
    std::size_t selectedQuickCommand_ = 0;
    bool editorHasError_ = false;
    std::array<char, kSshLabelCapacity> editorLabel_{};
    std::array<char, kSshHostnameCapacity> editorHostname_{};
    std::array<char, kSshUsernameCapacity> editorUsername_{};
    std::array<char, 6> editorPort_{};
};

}  // namespace pd
