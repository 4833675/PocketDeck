#include "apps/launcher/launcher_app.h"

#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* appTitle(AppId id) {
    return id == AppId::Keyboard ? "KEYBOARD" : "SETTINGS";
}

const char* appIcon(AppId id) {
    return id == AppId::Keyboard ? ">_" : "::";
}

}  // namespace

void LauncherApp::onEnter(SystemContext&) {}
void LauncherApp::onExit(SystemContext&) {}
void LauncherApp::update(uint32_t, SystemContext&) {}

void LauncherApp::onInput(const InputEvent& event, SystemContext& context) {
    const AppId requested = model_.handle(event.action);
    if (requested != AppId::None) context.requestApp(requested);
}

void LauncherApp::render(Display& display, const SystemContext& context) {
    auto& canvas = display.canvas();
    drawStatusBar(display, {"POCKET DECK", context.bleEnabled, context.bleConnected,
                            context.batteryPercent});

    constexpr int16_t cardX = 52;
    constexpr int16_t cardY = 27;
    constexpr int16_t cardW = 136;
    constexpr int16_t cardH = 76;
    canvas.drawRoundRect(-13, 38, 44, 54, 7, theme::kBorder);
    canvas.drawRoundRect(209, 38, 44, 54, 7, theme::kBorder);
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 8, theme::kPanelRaised);
    canvas.drawRoundRect(cardX, cardY, cardW, cardH, 8, theme::kPrimary);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kPrimary, theme::kPanelRaised);
    canvas.setTextSize(2);
    canvas.drawString(appIcon(model_.selected()), cardX + cardW / 2, cardY + 23);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    canvas.drawString(appTitle(model_.selected()), cardX + cardW / 2, cardY + 47);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    const char* subtitle = model_.selected() == AppId::Keyboard
                               ? (context.bleConnected ? "Mac connected" : "Ready to pair")
                               : "System controls";
    canvas.drawString(subtitle, cardX + cardW / 2, cardY + 62);

    const int16_t hintY = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, hintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.drawString("FN+, / FN+/  SELECT   ENTER OPEN", config::kScreenWidth / 2,
                      hintY + theme::kHintHeight / 2);
}

}  // namespace pd

