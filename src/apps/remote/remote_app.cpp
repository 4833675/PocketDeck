#include "apps/remote/remote_app.h"

#include <cstdio>

#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/diagnostics_service.h"
#include "services/ir_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* commandLogName(SonyIrCommand command) {
    switch (command) {
        case SonyIrCommand::Up: return "UP";
        case SonyIrCommand::Down: return "DOWN";
        case SonyIrCommand::Left: return "LEFT";
        case SonyIrCommand::Right: return "RIGHT";
        case SonyIrCommand::Ok: return "OK";
        case SonyIrCommand::Back: return "TV_BACK";
        case SonyIrCommand::Return: return "RETURN";
        case SonyIrCommand::Power: return "POWER";
        case SonyIrCommand::Home: return "HOME";
        case SonyIrCommand::Input: return "INPUT";
        case SonyIrCommand::Mute: return "MUTE";
        case SonyIrCommand::VolumeDown: return "VOLUME_DOWN";
        case SonyIrCommand::VolumeUp: return "VOLUME_UP";
        case SonyIrCommand::None: return "NONE";
    }
    return "NONE";
}

const char* commandUiLabel(SonyIrCommand command, UiLanguage language) {
    switch (command) {
        case SonyIrCommand::Up: return localized(language, "UP", "上");
        case SonyIrCommand::Down: return localized(language, "DOWN", "下");
        case SonyIrCommand::Left: return localized(language, "LEFT", "左");
        case SonyIrCommand::Right: return localized(language, "RIGHT", "右");
        case SonyIrCommand::Ok: return localized(language, "OK", "确定");
        case SonyIrCommand::Back: return localized(language, "TV BACK", "电视返回");
        case SonyIrCommand::Return: return localized(language, "RETURN", "返回");
        case SonyIrCommand::Power: return localized(language, "POWER", "电源");
        case SonyIrCommand::Home: return localized(language, "HOME", "主页");
        case SonyIrCommand::Input: return localized(language, "INPUT", "输入源");
        case SonyIrCommand::Mute: return localized(language, "MUTE", "静音");
        case SonyIrCommand::VolumeDown:
            return localized(language, "VOLUME DOWN", "音量-");
        case SonyIrCommand::VolumeUp:
            return localized(language, "VOLUME UP", "音量+");
        case SonyIrCommand::None: return localized(language, "NONE", "无");
    }
    return localized(language, "NONE", "无");
}

const char* localStateText(RemoteLocalState state, UiLanguage language) {
    switch (state) {
        case RemoteLocalState::None: return localized(language, "READY", "就绪");
        case RemoteLocalState::Sent: return localized(language, "SENT", "已发送");
        case RemoteLocalState::Error:
            return localized(language, "LOCAL ERROR", "本机发送失败");
    }
    return localized(language, "LOCAL ERROR", "本机发送失败");
}

uint16_t localStateColor(RemoteLocalState state) {
    switch (state) {
        case RemoteLocalState::None: return theme::kMuted;
        case RemoteLocalState::Sent: return theme::kPrimary;
        case RemoteLocalState::Error: return theme::kError;
    }
    return theme::kError;
}

}  // namespace

void RemoteApp::onEnter(SystemContext&) {}
void RemoteApp::onExit(SystemContext&) {}
void RemoteApp::update(uint32_t, SystemContext&) {}

void RemoteApp::onInput(const InputEvent& event, SystemContext& context) {
    const RemoteAppResult result = model_.handle(event);
    if (result.effect != RemoteAppEffect::SendSony) return;

    lastCommand_ = result.command;
    const bool sent = context.ir != nullptr && context.ir->send(result.command);
    localState_ = sent ? RemoteLocalState::Sent : RemoteLocalState::Error;
    if (context.diagnostics != nullptr) {
        context.diagnostics->logf("REMOTE %s local %s", commandLogName(result.command),
                                  sent ? "sent" : "failed");
    }
}

void RemoteApp::render(Display& display, const SystemContext& context) {
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display, makeStatusBarData(
                               localized(language, "REMOTE", "遥控器"), context));
    auto& canvas = display.canvas();

    setTechnicalFont(canvas);
    canvas.setTextDatum(top_center);
    canvas.setTextColor(theme::kSecondary, theme::kBackground);
    canvas.drawString("Sony KD-65X9100H", config::kScreenWidth / 2, 18);
    canvas.drawFastHLine(5, 29, config::kScreenWidth - 10, theme::kBorder);

    setUiFont(canvas, language);
    canvas.setTextColor(theme::kText, theme::kBackground);
    constexpr int16_t kMapY = 32;
    constexpr int16_t kMapStep = 14;
    canvas.drawString(localized(language, "FN+ARROWS NAV   ENTER OK",
                                "FN+方向 导航   ENTER 确定"),
                      config::kScreenWidth / 2, kMapY);
    canvas.drawString(localized(language, "BKSP TV BACK   ` RETURN",
                                "BKSP 电视返回   ` 返回"),
                      config::kScreenWidth / 2, kMapY + kMapStep);
    canvas.drawString(localized(language, "p POWER   h HOME   i INPUT",
                                "p 电源   h 主页   i 输入源"),
                      config::kScreenWidth / 2, kMapY + 2 * kMapStep);
    canvas.drawString(localized(language, "m MUTE   - VOL-   = VOL+",
                                "m 静音   - 音量-   = 音量+"),
                      config::kScreenWidth / 2, kMapY + 3 * kMapStep);

    constexpr int16_t kResultY = 89;
    canvas.fillRoundRect(4, kResultY, config::kScreenWidth - 8, 29, 5,
                         theme::kPanelRaised);
    canvas.drawRoundRect(4, kResultY, config::kScreenWidth - 8, 29, 5,
                         theme::kBorder);
    char last[48]{};
    std::snprintf(last, sizeof(last), localized(language, "LAST: %s", "上次：%s"),
                  commandUiLabel(lastCommand_, language));
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(last, config::kScreenWidth / 2, kResultY + 1);

    const char* state = localStateText(localState_, language);
    uint16_t color = localStateColor(localState_);
    if (context.ir == nullptr) {
        state = localized(language, "SERVICE UNAVAILABLE", "服务不可用");
        color = theme::kError;
    } else if (!context.ir->active()) {
        state = localized(language, "IR INACTIVE", "红外未启用");
        color = theme::kWarning;
    }
    canvas.setTextColor(color, theme::kPanelRaised);
    canvas.drawString(state, config::kScreenWidth / 2, kResultY + 15);
}

}  // namespace pd
