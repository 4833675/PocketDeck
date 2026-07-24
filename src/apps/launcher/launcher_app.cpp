#include "apps/launcher/launcher_app.h"

#include <cstdio>

#include "core/gps_data.h"
#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/gps_service.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

const char* appTitle(AppId id) {
    switch (id) {
        case AppId::Keyboard: return "KEYBOARD";
        case AppId::Gps: return "GPS";
        case AppId::Settings: return "SETTINGS";
        default: return "APP";
    }
}

const char* appIcon(AppId id) {
    switch (id) {
        case AppId::Keyboard: return ">_";
        case AppId::Gps: return "G+";
        case AppId::Settings: return "::";
        default: return "--";
    }
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
    char subtitle[32];
    if (model_.selected() == AppId::Keyboard) {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      context.bleConnected ? "Mac connected" : "Ready to pair");
    } else if (model_.selected() == AppId::Gps) {
        const GpsSnapshot gps = context.gps != nullptr ? context.gps->snapshot() : GpsSnapshot{};
        if (classifyGpsState(gps) == GpsState::Fix && gps.satellitesValid) {
            std::snprintf(subtitle, sizeof(subtitle), "Fix / %lu satellites",
                          static_cast<unsigned long>(gps.satellites));
        } else {
            std::snprintf(subtitle, sizeof(subtitle), "%s", gpsStateLabel(classifyGpsState(gps)));
        }
    } else {
        std::snprintf(subtitle, sizeof(subtitle), "System controls");
    }
    canvas.drawString(subtitle, cardX + cardW / 2, cardY + 62);

    const int16_t hintY = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, hintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.drawString("FN+, / FN+/  SELECT   ENTER OPEN", config::kScreenWidth / 2,
                      hintY + theme::kHintHeight / 2);
}

}  // namespace pd
