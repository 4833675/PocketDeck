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

}  // namespace

void GpsApp::onEnter(SystemContext&) {
    page_ = 0;
}

void GpsApp::onExit(SystemContext&) {}
void GpsApp::update(uint32_t, SystemContext&) {}

void GpsApp::onInput(const InputEvent& event, SystemContext& context) {
    if (event.action == InputAction::Left) {
        page_ = static_cast<uint8_t>((page_ + 2) % 3);
    } else if (event.action == InputAction::Right || event.action == InputAction::Tab) {
        page_ = static_cast<uint8_t>((page_ + 1) % 3);
    } else if (event.action == InputAction::Back) {
        context.requestApp(AppId::Launcher);
    }
}

void GpsApp::render(Display& display, const SystemContext& context) {
    const GpsSnapshot gps = context.gps != nullptr ? context.gps->snapshot() : GpsSnapshot{};
    char title[10];
    std::snprintf(title, sizeof(title), "GPS %u/3", static_cast<unsigned>(page_ + 1));
    drawStatusBar(display, makeStatusBarData(title, context));
    auto& canvas = display.canvas();
    if (page_ == 0) {
        drawPosition(canvas, gps);
    } else if (page_ == 1) {
        drawMotionAndTime(canvas, gps);
    } else {
        drawReceiver(canvas, gps);
    }
    drawHint(canvas);
}

}  // namespace pd
