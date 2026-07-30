#include "apps/ssh/ssh_app_model.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pd {

void SshAppModel::reset() {
    page_ = SshPage::HostList;
    selectedHost_ = 0;
    selectedField_ = 0;
    editingIndex_ = kNewHostIndex;
    selectedQuickCommand_ = 0;
    editorHasError_ = false;
}

InputMode SshAppModel::inputMode() const {
    if (page_ == SshPage::HostList || page_ == SshPage::Editor) return InputMode::Text;
    if (page_ == SshPage::Terminal) return InputMode::Terminal;
    return InputMode::System;
}

SshResult SshAppModel::handle(const InputEvent& event, std::size_t hostCount) {
    if (page_ == SshPage::Connecting) {
        if (event.action == InputAction::Back) {
            page_ = SshPage::HostList;
            return {SshEffect::CancelConnection};
        }
        return {};
    }
    if (page_ == SshPage::Terminal) {
        if (event.action == InputAction::QuickCommands) {
            selectedQuickCommand_ = 0;
            page_ = SshPage::QuickCommands;
        }
        return {};
    }
    if (page_ == SshPage::QuickCommands) {
        constexpr std::size_t count = 4;
        if (event.action == InputAction::Up) {
            selectedQuickCommand_ = (selectedQuickCommand_ + count - 1) % count;
        } else if (event.action == InputAction::Down) {
            selectedQuickCommand_ = (selectedQuickCommand_ + 1) % count;
        } else if (event.action == InputAction::Confirm) {
            page_ = SshPage::Terminal;
            return {SshEffect::SendQuickCommand};
        } else if (event.action == InputAction::Back) {
            page_ = SshPage::Terminal;
        }
        return {};
    }
    if (page_ == SshPage::Disconnected) {
        if (event.action == InputAction::Confirm) return {SshEffect::Reconnect};
        if (event.action == InputAction::Back) page_ = SshPage::HostList;
        return {};
    }
    if (page_ == SshPage::Editor) {
        if (event.action == InputAction::Back) {
            page_ = SshPage::HostList;
        } else if (event.action == InputAction::Erase) {
            eraseEditorCharacter();
        } else if (event.action == InputAction::Up) {
            moveField(-1);
        } else if (event.action == InputAction::Down || event.action == InputAction::Tab) {
            moveField(1);
        } else if (event.action == InputAction::Confirm) {
            if (selectedField_ < 3) {
                ++selectedField_;
            } else {
                SshHost host;
                if (editedHost(host)) return {SshEffect::SaveEditor};
                editorHasError_ = true;
            }
        } else if (event.character != '\0') {
            appendEditorCharacter(event.character);
            editorHasError_ = false;
        }
        return {};
    }
    if (page_ == SshPage::ConfirmDelete) {
        if (event.action == InputAction::Back) {
            page_ = SshPage::HostList;
        } else if (event.action == InputAction::Confirm && hostCount > 0) {
            const std::size_t deleted = selectedHost_;
            const std::size_t remaining = hostCount - 1;
            if (remaining == 0) {
                selectedHost_ = 0;
            } else if (selectedHost_ >= remaining) {
                selectedHost_ = remaining - 1;
            }
            page_ = SshPage::HostList;
            return {SshEffect::DeleteSelected, deleted};
        }
        return {};
    }
    if (page_ != SshPage::HostList) return {};

    if (event.action == InputAction::Up) {
        moveSelection(-1, hostCount);
    } else if (event.action == InputAction::Down) {
        moveSelection(1, hostCount);
    } else if (event.action == InputAction::Confirm && hostCount > 0) {
        return {SshEffect::ConnectSelected, selectedHost_};
    } else if (event.action == InputAction::Back || event.action == InputAction::Erase) {
        return {SshEffect::GoHome};
    } else if ((event.character == 'e' || event.character == 'E') && hostCount > 0) {
        return {SshEffect::EditSelected, selectedHost_};
    } else if ((event.character == 'd' || event.character == 'D') && hostCount > 0) {
        page_ = SshPage::ConfirmDelete;
    } else if (event.character == 'n' || event.character == 'N') {
        startNewEditor();
    }
    return {};
}

const char* SshAppModel::quickCommand() const {
    static constexpr const char* commands[] = {"uptime", "df -h", "free -h", "docker ps"};
    return commands[selectedQuickCommand_ % 4];
}

const char* SshAppModel::editorValue(std::size_t field) const {
    if (field == 0) return editorLabel_.data();
    if (field == 1) return editorHostname_.data();
    if (field == 2) return editorUsername_.data();
    if (field == 3) return editorPort_.data();
    return "";
}

bool SshAppModel::editedHost(SshHost& host) const {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(editorPort_.data(), &end, 10);
    if (end == editorPort_.data() || *end != '\0' || parsed == 0 || parsed > 65535) {
        return false;
    }
    SshHosts validated;
    if (!validated.upsert(editorLabel_.data(), editorHostname_.data(),
                          editorUsername_.data(), static_cast<uint16_t>(parsed))) {
        return false;
    }
    host = validated.at(0);
    return true;
}

void SshAppModel::beginEdit(const SshHost& host, std::size_t index) {
    editorLabel_ = host.label;
    editorHostname_ = host.hostname;
    editorUsername_ = host.username;
    editorPort_.fill('\0');
    std::snprintf(editorPort_.data(), editorPort_.size(), "%u",
                  static_cast<unsigned>(host.port));
    selectedField_ = 0;
    editingIndex_ = index;
    editorHasError_ = false;
    page_ = SshPage::Editor;
}

void SshAppModel::startNewEditor() {
    editorLabel_.fill('\0');
    editorHostname_.fill('\0');
    editorUsername_.fill('\0');
    editorPort_.fill('\0');
    std::strncpy(editorPort_.data(), "22", editorPort_.size() - 1);
    selectedField_ = 0;
    editingIndex_ = kNewHostIndex;
    editorHasError_ = false;
    page_ = SshPage::Editor;
}

void SshAppModel::moveField(int direction) {
    constexpr int count = 4;
    selectedField_ = static_cast<std::size_t>(
        (static_cast<int>(selectedField_) + direction + count) % count);
}

void SshAppModel::appendEditorCharacter(char character) {
    char* buffer = nullptr;
    std::size_t capacity = 0;
    if (selectedField_ == 0) {
        buffer = editorLabel_.data();
        capacity = editorLabel_.size();
    } else if (selectedField_ == 1) {
        buffer = editorHostname_.data();
        capacity = editorHostname_.size();
    } else if (selectedField_ == 2) {
        buffer = editorUsername_.data();
        capacity = editorUsername_.size();
    } else {
        if (character < '0' || character > '9') return;
        buffer = editorPort_.data();
        capacity = editorPort_.size();
    }
    const std::size_t length = std::strlen(buffer);
    if (length + 1 >= capacity || character < 0x20 || character > 0x7E) return;
    buffer[length] = character;
    buffer[length + 1] = '\0';
}

void SshAppModel::eraseEditorCharacter() {
    char* buffer = nullptr;
    if (selectedField_ == 0) buffer = editorLabel_.data();
    else if (selectedField_ == 1) buffer = editorHostname_.data();
    else if (selectedField_ == 2) buffer = editorUsername_.data();
    else buffer = editorPort_.data();
    const std::size_t length = std::strlen(buffer);
    if (length > 0) buffer[length - 1] = '\0';
}

void SshAppModel::moveSelection(int direction, std::size_t count) {
    if (count == 0) {
        selectedHost_ = 0;
        return;
    }
    const auto next = static_cast<std::size_t>(
        (static_cast<int>(selectedHost_) + direction + static_cast<int>(count)) %
        static_cast<int>(count));
    selectedHost_ = next;
}

}  // namespace pd
