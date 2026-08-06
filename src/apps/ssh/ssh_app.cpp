#include "apps/ssh/ssh_app.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "apps/ssh/ssh_app_text.h"
#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "core/terminal_input.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/diagnostics_service.h"
#include "services/ssh_host_store.h"
#include "services/ssh_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

void drawHint(M5Canvas& canvas, UiLanguage language, const char* english,
              const char* simplifiedChinese) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(localized(language, english, simplifiedChinese),
                      config::kScreenWidth / 2, y + theme::kHintHeight / 2);
}

uint16_t ansiColor(uint8_t color) {
    return theme::kAnsiPalette[color & 0x0Fu];
}

void drawTerminal(M5Canvas& canvas, const TerminalBuffer& terminal, bool connected,
                  UiLanguage language) {
    constexpr int16_t lineHeight = 8;
    constexpr int16_t cellWidth = 6;
    constexpr int16_t top = theme::kStatusHeight + 1;
    setTechnicalFont(canvas);
    canvas.setTextDatum(top_left);

    for (std::size_t row = 0; row < kTerminalRows; ++row) {
        std::size_t start = 0;
        while (start < kTerminalColumns) {
            const uint8_t style = terminal.cell(row, start).style;
            std::size_t end = start + 1;
            while (end < kTerminalColumns && terminal.cell(row, end).style == style) ++end;
            std::array<char, kTerminalColumns + 1> text{};
            bool visible = false;
            for (std::size_t column = start; column < end; ++column) {
                text[column - start] = terminal.cell(row, column).character;
                if (text[column - start] != ' ') visible = true;
            }
            const uint16_t foreground = ansiColor(style & 0x0Fu);
            const uint16_t background = ansiColor((style >> 4u) & 0x0Fu);
            const int16_t x = static_cast<int16_t>(start * cellWidth);
            const int16_t y = static_cast<int16_t>(top + row * lineHeight);
            if (((style >> 4u) & 0x0Fu) != 0) {
                canvas.fillRect(x, y, static_cast<int16_t>((end - start) * cellWidth),
                                lineHeight, background);
            }
            if (visible) {
                canvas.setTextColor(foreground, background);
                canvas.drawString(text.data(), x, y);
            }
            start = end;
        }
    }

    if (terminal.scrollOffset() > 0) {
        char marker[24];
        if (isSimplifiedChinese(language)) {
            std::snprintf(marker, sizeof(marker), "历史 %u",
                          static_cast<unsigned>(terminal.scrollOffset()));
        } else {
            std::snprintf(marker, sizeof(marker), "HISTORY %u",
                          static_cast<unsigned>(terminal.scrollOffset()));
        }
        canvas.fillRoundRect(167, 18, 70, 12, 2, theme::kPanelRaised);
        setUiFont(canvas, language);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(theme::kWarning, theme::kPanelRaised);
        canvas.drawString(marker, 202, 24);
    } else if (connected) {
        const int16_t x = static_cast<int16_t>(terminal.cursorColumn() * cellWidth);
        const int16_t y = static_cast<int16_t>(top + terminal.cursorRow() * lineHeight + 7);
        canvas.drawFastHLine(x, y, cellWidth, theme::kPrimary);
    }
}

bool autoRetryable(SshError error) {
    // Retry automatically only when the user connected before Wi-Fi was ready.
    // Repeating failed SSH handshakes can trigger OpenSSH per-source penalties;
    // transport and remote-close failures therefore wait for explicit Enter.
    return error == SshError::NoNetwork;
}

void drawHostList(M5Canvas& canvas, const SshAppModel& model, const SshHosts& hosts,
                  const SshSnapshot& ssh, bool storeError, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    if (!ssh.keyAvailable) {
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString(localized(language, "SSH PRIVATE KEY MISSING", "缺少 SSH 私钥"),
                          8, 28);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Rebuild with POCKETDECK_SSH_KEY",
                                    "请用 POCKETDECK_SSH_KEY"),
                          8, 49);
        canvas.drawString(localized(language, "or ~/.ssh/id_rsa",
                                    "或 ~/.ssh/id_rsa 重新构建"),
                          8, 65);
    } else if (hosts.empty()) {
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(theme::kPrimary, theme::kBackground);
        if (!isSimplifiedChinese(language)) canvas.setTextSize(2);
        canvas.drawString(localized(language, "NO SSH HOSTS", "没有 SSH 主机"),
                          config::kScreenWidth / 2, 49);
        setUiFont(canvas, language);
        canvas.setTextColor(theme::kMuted, theme::kBackground);
        canvas.drawString(localized(language, "Press N to add your first host",
                                    "按 N 添加第一台主机"),
                          config::kScreenWidth / 2, 74);
    } else {
        setTechnicalFont(canvas);
        const std::size_t selected = std::min(model.selectedHost(), hosts.size() - 1);
        const std::size_t start = selected >= 5 ? selected - 4 : 0;
        const std::size_t end = std::min(hosts.size(), start + 5);
        for (std::size_t index = start; index < end; ++index) {
            const bool active = index == selected;
            const int16_t y = static_cast<int16_t>(22 + (index - start) * 19);
            const uint16_t background = active ? theme::kPanelRaised : theme::kBackground;
            canvas.fillRoundRect(5, y, 230, 17, 3, background);
            if (active) canvas.drawRoundRect(5, y, 230, 17, 3, theme::kPrimary);
            char line[48];
            const SshHost& host = hosts.at(index);
            std::snprintf(line, sizeof(line), "%-12.12s %.9s@%.14s:%u",
                          host.label.data(), host.username.data(), host.hostname.data(),
                          static_cast<unsigned>(host.port));
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(active ? theme::kText : theme::kMuted, background);
            canvas.drawString(line, 10, y + 8);
        }
    }
    if (storeError) {
        setTechnicalFont(canvas);
        canvas.setTextDatum(top_right);
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString("NVS!", 235, 18);
    }
    drawHint(canvas, language, "N NEW  E EDIT  D DELETE  ENTER CONNECT",
             "N 新建  E 编辑  D 删除  ENTER 连接");
}

void drawEditor(M5Canvas& canvas, const SshAppModel& model, UiLanguage language) {
    const char* labels[] = {
        localized(language, "LABEL", "名称"),
        localized(language, "HOST / IP", "主机 / IP"),
        localized(language, "USERNAME", "用户名"),
        localized(language, "PORT", "端口"),
    };
    static constexpr int16_t yPositions[] = {21, 45, 69, 93};
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    for (std::size_t field = 0; field < 4; ++field) {
        const bool selected = model.selectedField() == field;
        const int16_t y = yPositions[field];
        const uint16_t background = selected ? theme::kPanelRaised : theme::kPanel;
        canvas.fillRoundRect(6, y, 228, 21, 3, background);
        if (selected) canvas.drawRoundRect(6, y, 228, 21, 3, theme::kPrimary);
        canvas.setTextColor(selected ? theme::kPrimary : theme::kMuted, background);
        canvas.drawString(labels[field], 10, y + 2);
        setTechnicalFont(canvas);
        canvas.setTextColor(theme::kText, background);
        char value[34];
        std::snprintf(value, sizeof(value), "%.32s", model.editorValue(field));
        canvas.setTextDatum(top_right);
        canvas.drawString(value[0] != '\0' ? value : "--", 229, y + 8);
        setUiFont(canvas, language);
        canvas.setTextDatum(top_left);
    }
    if (model.editorHasError()) {
        canvas.setTextDatum(top_right);
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString(localized(language, "CHECK ALL FIELDS", "请检查全部字段"),
                          235, 106);
    }
    drawHint(canvas, language, "TAB FIELD  ENTER NEXT/SAVE  FN+` CANCEL",
             "TAB 字段  ENTER 下一项/保存  FN+` 取消");
}

void drawCentered(M5Canvas& canvas, const char* title, const char* detail,
                  uint16_t color, UiLanguage language) {
    canvas.setTextDatum(middle_center);
    setUiFont(canvas, language);
    if (!isSimplifiedChinese(language)) canvas.setTextSize(2);
    canvas.setTextColor(color, theme::kBackground);
    canvas.drawString(title, config::kScreenWidth / 2, 45);
    setFontForText(canvas, detail);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(detail, config::kScreenWidth / 2, 72);
}

}  // namespace

void SshApp::onEnter(SystemContext& context) {
    model_.reset();
    terminal_.reset();
    hasActiveHost_ = false;
    retryPolicy_.cancel();
    if (!hostsLoaded_ && context.sshHostStore != nullptr) {
        const SshHostLoadResult loaded = context.sshHostStore->load();
        if (loaded.valid) hosts_ = loaded.hosts;
        storeError_ = !loaded.storageReady || (loaded.found && !loaded.valid);
        hostsLoaded_ = true;
    }
    if (context.ssh != nullptr) {
        serviceReady_ = context.ssh->begin(context.diagnostics);
        lastSshGeneration_ = context.ssh->snapshot().generation;
    }
}

void SshApp::onExit(SystemContext& context) {
    if (context.ssh != nullptr) context.ssh->disconnect();
    hasActiveHost_ = false;
    retryPolicy_.cancel();
    model_.reset();
}

void SshApp::onInput(const InputEvent& event, SystemContext& context) {
    if (model_.page() == SshPage::Terminal) {
        if (event.action == InputAction::ScrollUp) {
            terminal_.scrollUp(3);
            return;
        }
        if (event.action == InputAction::ScrollDown) {
            terminal_.scrollDown(3);
            return;
        }
        if (event.action == InputAction::QuickCommands) {
            handleResult(model_.handle(event, hosts_.size()), context);
            return;
        }
        const TerminalInput input = encodeTerminalInput(event);
        if (!input.empty() && context.ssh != nullptr) {
            context.ssh->send(input.data(), input.length);
        }
        return;
    }
    handleResult(model_.handle(event, hosts_.size()), context);
}

void SshApp::update(uint32_t nowMs, SystemContext& context) {
    if (context.ssh == nullptr || !serviceReady_) return;
    std::array<uint8_t, 256> incoming{};
    std::size_t total = 0;
    while (total < 2048) {
        const std::size_t count = context.ssh->read(incoming.data(), incoming.size());
        if (count == 0) break;
        terminal_.write(incoming.data(), count);
        total += count;
    }

    const SshSnapshot snapshot = context.ssh->snapshot();
    if (snapshot.generation != lastSshGeneration_) {
        lastSshGeneration_ = snapshot.generation;
        if (snapshot.state == SshState::Connected) {
            retryPolicy_.cancel();
            model_.showTerminal();
        } else if ((snapshot.state == SshState::Error ||
                    snapshot.state == SshState::Disconnected) &&
                   hasActiveHost_) {
            model_.showDisconnected();
            if (autoRetryable(snapshot.error)) {
                retryPolicy_.noteFailure(nowMs);
            } else {
                retryPolicy_.cancel();
            }
        }
    }

    if (model_.page() == SshPage::Disconnected && hasActiveHost_ &&
        context.wifiConnected && autoRetryable(snapshot.error) &&
        retryPolicy_.takeDue(nowMs)) {
        reconnect(context);
    }
}

void SshApp::render(Display& display, const SystemContext& context) {
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display, makeStatusBarData("SSH", context));
    auto& canvas = display.canvas();
    const SshSnapshot snapshot = context.ssh != nullptr ? context.ssh->snapshot()
                                                        : SshSnapshot{};
    switch (model_.page()) {
        case SshPage::HostList:
            drawHostList(canvas, model_, hosts_, snapshot, storeError_, language);
            return;
        case SshPage::Editor:
            drawEditor(canvas, model_, language);
            return;
        case SshPage::ConfirmDelete: {
            const char* label = model_.selectedHost() < hosts_.size()
                                    ? hosts_.at(model_.selectedHost()).label.data()
                                    : localized(language, "host", "主机");
            drawCentered(canvas, localized(language, "DELETE HOST?", "删除主机？"),
                         label, theme::kError, language);
            drawHint(canvas, language, "ENTER DELETE   DEL CANCEL",
                     "ENTER 删除   DEL 取消");
            return;
        }
        case SshPage::Connecting:
            drawCentered(canvas, localizedSshStateLabel(snapshot.state, language),
                         activeHost_.label.data(), theme::kWarning, language);
            setUiFont(canvas, language);
            canvas.setTextDatum(middle_center);
            canvas.setTextColor(theme::kMuted, theme::kBackground);
            canvas.drawString(localized(language, "HOST KEY CHECK: OFF",
                                        "主机密钥校验：关闭"),
                              config::kScreenWidth / 2, 91);
            drawHint(canvas, language, "DEL CANCEL   G0 HOME",
                     "DEL 取消   G0 主页");
            return;
        case SshPage::Disconnected: {
            drawCentered(canvas, localizedSshStateLabel(snapshot.state, language),
                         localizedSshErrorLabel(snapshot.error, language),
                         snapshot.error == SshError::Authentication ? theme::kError
                                                                    : theme::kWarning,
                         language);
            if (autoRetryable(snapshot.error) && context.wifiConnected) {
                const uint32_t remaining = retryPolicy_.secondsRemaining(context.uptimeMs);
                char retry[48];
                if (isSimplifiedChinese(language)) {
                    std::snprintf(retry, sizeof(retry), "自动重试 %lu 秒",
                                  static_cast<unsigned long>(remaining));
                } else {
                    std::snprintf(retry, sizeof(retry), "Auto retry in %lus",
                                  static_cast<unsigned long>(remaining));
                }
                setUiFont(canvas, language);
                canvas.setTextDatum(middle_center);
                canvas.setTextColor(theme::kMuted, theme::kBackground);
                canvas.drawString(retry, config::kScreenWidth / 2, 91);
            }
            drawHint(canvas, language, "ENTER RETRY   DEL HOSTS   G0 HOME",
                     "ENTER 重试   DEL 主机   G0 主页");
            return;
        }
        case SshPage::Terminal:
            drawTerminal(canvas, terminal_, snapshot.state == SshState::Connected, language);
            return;
        case SshPage::QuickCommands:
            drawTerminal(canvas, terminal_, snapshot.state == SshState::Connected, language);
            canvas.fillRoundRect(25, 26, 190, 82, 6, theme::kPanelRaised);
            canvas.drawRoundRect(25, 26, 190, 82, 6, theme::kPrimary);
            setTechnicalFont(canvas);
            canvas.setTextDatum(top_left);
            for (std::size_t index = 0; index < 4; ++index) {
                const bool active = index == model_.selectedQuickCommand();
                const int16_t y = static_cast<int16_t>(33 + index * 17);
                if (active) canvas.fillRoundRect(32, y, 176, 15, 3, theme::kPrimary);
                const char* commands[] = {"uptime", "df -h", "free -h", "docker ps"};
                canvas.setTextColor(active ? theme::kBackground : theme::kText,
                                    active ? theme::kPrimary : theme::kPanelRaised);
                canvas.drawString(commands[index], 39, y + 3);
            }
            return;
    }
}

void SshApp::handleResult(const SshResult& result, SystemContext& context) {
    switch (result.effect) {
        case SshEffect::None: return;
        case SshEffect::GoHome:
            context.requestApp(AppId::Launcher);
            return;
        case SshEffect::EditSelected:
            if (result.index < hosts_.size()) model_.beginEdit(hosts_.at(result.index), result.index);
            return;
        case SshEffect::ConnectSelected:
            connectHost(result.index, context);
            return;
        case SshEffect::SaveEditor: {
            SshHost edited;
            if (!model_.editedHost(edited)) return;
            bool changed = false;
            if (model_.editingIndex() == SshAppModel::kNewHostIndex) {
                changed = hosts_.upsert(edited.label.data(), edited.hostname.data(),
                                        edited.username.data(), edited.port);
            } else {
                changed = hosts_.update(model_.editingIndex(), edited.label.data(),
                                        edited.hostname.data(), edited.username.data(),
                                        edited.port);
            }
            if (changed) {
                saveHosts(context);
                model_.showHostList();
            }
            return;
        }
        case SshEffect::DeleteSelected:
            if (hosts_.erase(result.index)) saveHosts(context);
            return;
        case SshEffect::CancelConnection:
            retryPolicy_.cancel();
            if (context.ssh != nullptr) context.ssh->disconnect();
            return;
        case SshEffect::Reconnect:
            reconnect(context);
            return;
        case SshEffect::SendQuickCommand:
            if (context.ssh != nullptr) {
                const char* command = model_.quickCommand();
                context.ssh->send(reinterpret_cast<const uint8_t*>(command),
                                  std::strlen(command));
                constexpr uint8_t enter = '\r';
                context.ssh->send(&enter, 1);
            }
            return;
    }
}

void SshApp::connectHost(std::size_t index, SystemContext& context) {
    if (index >= hosts_.size() || context.ssh == nullptr || !serviceReady_) return;
    activeHost_ = hosts_.at(index);
    hasActiveHost_ = true;
    hosts_.touch(index);
    saveHosts(context);
    terminal_.reset();
    retryPolicy_.cancel();
    if (context.ssh->connect(activeHost_)) {
        model_.showConnecting();
    } else {
        model_.showDisconnected();
    }
}

void SshApp::reconnect(SystemContext& context) {
    if (!hasActiveHost_ || context.ssh == nullptr || !serviceReady_) return;
    retryPolicy_.cancel();
    if (context.ssh->connect(activeHost_)) model_.showConnecting();
}

bool SshApp::saveHosts(SystemContext& context) {
    if (context.sshHostStore == nullptr || !context.sshHostStore->save(hosts_)) {
        storeError_ = true;
        return false;
    }
    storeError_ = false;
    return true;
}

}  // namespace pd
