#include "apps/gps/gps_app.h"

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

uint16_t stateColor(GpsState state) {
    switch (state) {
        case GpsState::Fix: return theme::kPrimary;
        case GpsState::Searching: return theme::kWarning;
        case GpsState::Stale: return theme::kWarning;
        case GpsState::NoData: return theme::kMuted;
        case GpsState::NoStream: return theme::kError;
    }
    return theme::kMuted;
}

void drawHint(M5Canvas& canvas) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString("FN+, / FN+/ PAGE   DEL HOME", config::kScreenWidth / 2,
                      y + theme::kHintHeight / 2);
}

void drawState(M5Canvas& canvas, const GpsSnapshot& gps) {
    const GpsState state = classifyGpsState(gps);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.setTextColor(stateColor(state), theme::kBackground);
    canvas.drawString(gpsStateLabel(state), config::kScreenWidth / 2, 28);
    canvas.setTextSize(1);
}

void drawPosition(M5Canvas& canvas, const GpsSnapshot& gps) {
    drawState(canvas, gps);
    char line[48];
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);

    if (gps.locationValid) {
        std::snprintf(line, sizeof(line), "LAT   %+.6f", gps.latitude);
    } else {
        std::snprintf(line, sizeof(line), "LAT   --");
    }
    canvas.drawString(line, 8, 42);

    if (gps.locationValid) {
        std::snprintf(line, sizeof(line), "LON   %+.6f", gps.longitude);
    } else {
        std::snprintf(line, sizeof(line), "LON   --");
    }
    canvas.drawString(line, 8, 56);

    if (gps.altitudeValid) {
        std::snprintf(line, sizeof(line), "ALT   %.1f m", gps.altitudeMeters);
    } else {
        std::snprintf(line, sizeof(line), "ALT   --");
    }
    canvas.drawString(line, 8, 70);

    if (gps.satellitesValid) {
        std::snprintf(line, sizeof(line), "SAT   %lu", static_cast<unsigned long>(gps.satellites));
    } else {
        std::snprintf(line, sizeof(line), "SAT   --");
    }
    canvas.drawString(line, 8, 84);

    if (gps.hdopValid) {
        std::snprintf(line, sizeof(line), "HDOP %.2f", gps.hdop);
    } else {
        std::snprintf(line, sizeof(line), "HDOP --");
    }
    canvas.drawString(line, 124, 84);

    canvas.setTextColor(theme::kMuted, theme::kBackground);
    if (gps.locationValid) {
        std::snprintf(line, sizeof(line), "FIX AGE %.1fs", gps.locationAgeMs / 1000.0);
    } else {
        std::snprintf(line, sizeof(line), "Waiting for position");
    }
    canvas.drawString(line, 8, 101);
    if (gps.charsProcessed > 0) {
        std::snprintf(line, sizeof(line), "DATA %.1fs", gps.dataAgeMs / 1000.0);
        canvas.setTextDatum(top_right);
        canvas.drawString(line, config::kScreenWidth - 8, 101);
    }
}

void drawMotionAndTime(M5Canvas& canvas, const GpsSnapshot& gps) {
    drawState(canvas, gps);
    char line[56];
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);

    if (gps.dateValid) {
        std::snprintf(line, sizeof(line), "UTC DATE  %04u-%02u-%02u", gps.year, gps.month, gps.day);
    } else {
        std::snprintf(line, sizeof(line), "UTC DATE  --");
    }
    canvas.drawString(line, 8, 42);

    if (gps.timeValid) {
        std::snprintf(line, sizeof(line), "UTC TIME  %02u:%02u:%02u.%02u", gps.hour, gps.minute,
                      gps.second, gps.centisecond);
    } else {
        std::snprintf(line, sizeof(line), "UTC TIME  --");
    }
    canvas.drawString(line, 8, 56);

    if (gps.speedValid) {
        std::snprintf(line, sizeof(line), "SPEED     %.1f km/h", gps.speedKph);
    } else {
        std::snprintf(line, sizeof(line), "SPEED     --");
    }
    canvas.drawString(line, 8, 70);

    if (gps.courseValid) {
        std::snprintf(line, sizeof(line), "COURSE    %.1f deg %s", gps.courseDegrees,
                      gpsCompassPoint(gps.courseDegrees));
    } else {
        std::snprintf(line, sizeof(line), "COURSE    --");
    }
    canvas.drawString(line, 8, 84);

    canvas.setTextColor(theme::kMuted, theme::kBackground);
    std::snprintf(line, sizeof(line), "MODE %s   QUALITY %s", gpsFixModeLabel(gps.fixMode),
                  gpsFixQualityLabel(gps.fixQuality));
    canvas.drawString(line, 8, 101);
}

void drawReceiver(M5Canvas& canvas, const GpsSnapshot& gps) {
    drawState(canvas, gps);
    char line[56];
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString("UART 115200  RX15  TX13", 8, 42);
    canvas.drawString("NMEA 0183 4.1 / TinyGPS+", 8, 56);

    std::snprintf(line, sizeof(line), "RX CHARS       %lu",
                  static_cast<unsigned long>(gps.charsProcessed));
    canvas.drawString(line, 8, 70);
    std::snprintf(line, sizeof(line), "CHECKSUM OK    %lu",
                  static_cast<unsigned long>(gps.sentencesPassed));
    canvas.drawString(line, 8, 84);
    std::snprintf(line, sizeof(line), "CHECKSUM ERR   %lu",
                  static_cast<unsigned long>(gps.checksumFailed));
    canvas.drawString(line, 8, 98);

    std::snprintf(line, sizeof(line), "FIX SENT %lu", static_cast<unsigned long>(gps.sentencesWithFix));
    canvas.setTextDatum(top_right);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(line, config::kScreenWidth - 8, 98);
}

void drawMotion(M5Canvas& canvas, const GpsSnapshot& gps) {
    const bool speedAvailable =
        gps.speedValid && classifyGpsState(gps) == GpsState::Fix;
    const bool courseAvailable =
        gps.courseValid && classifyGpsState(gps) == GpsState::Fix;
    char line[24];

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString("SPEED", config::kScreenWidth / 2, 27);
    canvas.setTextSize(3);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    if (speedAvailable) {
        std::snprintf(line, sizeof(line), "%.1f", gps.speedKph);
    } else {
        std::snprintf(line, sizeof(line), "--");
    }
    canvas.drawString(line, config::kScreenWidth / 2, 47);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString("KM/H", config::kScreenWidth / 2, 65);

    canvas.drawString("COURSE", config::kScreenWidth / 2, 77);
    canvas.setTextSize(3);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    if (courseAvailable) {
        std::snprintf(line, sizeof(line), "%.0f%s %s", gps.courseDegrees, "\xC2\xB0",
                      gpsCompassPoint(gps.courseDegrees));
    } else {
        std::snprintf(line, sizeof(line), "--");
    }
    canvas.drawString(line, config::kScreenWidth / 2, 97);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
}

}  // namespace

void GpsApp::onEnter(SystemContext&) {
    model_.reset();
}

void GpsApp::onExit(SystemContext&) {}
void GpsApp::update(uint32_t, SystemContext&) {}

void GpsApp::onInput(const InputEvent& event, SystemContext& context) {
    if (event.action == InputAction::Back) {
        context.requestApp(AppId::Launcher);
    } else {
        model_.handle(event.action);
    }
}

void GpsApp::render(Display& display, const SystemContext& context) {
    const GpsSnapshot gps = context.gps != nullptr ? context.gps->snapshot() : GpsSnapshot{};
    const GpsPage page = model_.page();
    char title[10];
    std::snprintf(title, sizeof(title), "GPS %u/4", static_cast<unsigned>(page) + 1);
    drawStatusBar(display, makeStatusBarData(title, context));
    auto& canvas = display.canvas();
    if (page == GpsPage::Position) {
        drawPosition(canvas, gps);
    } else if (page == GpsPage::Time) {
        drawMotionAndTime(canvas, gps);
    } else if (page == GpsPage::Receiver) {
        drawReceiver(canvas, gps);
    } else {
        drawMotion(canvas, gps);
    }
    drawHint(canvas);
}

}  // namespace pd
