#include "apps/gps/gps_app.h"
#include "apps/gps/gps_app_text.h"

#include <cstdio>

#include "core/gps_data.h"
#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/gps_service.h"
#include "ui/localized_font.h"
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

void drawHint(M5Canvas& canvas, UiLanguage language) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(localized(language, "FN+, / FN+/ PAGE   DEL HOME",
                                "FN+, / FN+/ 翻页   DEL 返回"),
                      config::kScreenWidth / 2, y + theme::kHintHeight / 2);
}

void drawState(M5Canvas& canvas, const GpsSnapshot& gps, UiLanguage language) {
    const GpsState state = classifyGpsState(gps);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(isSimplifiedChinese(language) ? 1 : 2);
    canvas.setTextColor(stateColor(state), theme::kBackground);
    canvas.drawString(localizedGpsStateLabel(state, language),
                      config::kScreenWidth / 2, 28);
    setUiFont(canvas, language);
}

void drawPosition(M5Canvas& canvas, const GpsSnapshot& gps, UiLanguage language) {
    drawState(canvas, gps, language);
    char line[64];
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);

    if (gps.locationValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "LAT   %+.6f", "纬度  %+.6f"),
                      gps.latitude);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "LAT   --", "纬度  --"));
    }
    canvas.drawString(line, 8, 42);

    if (gps.locationValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "LON   %+.6f", "经度  %+.6f"),
                      gps.longitude);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "LON   --", "经度  --"));
    }
    canvas.drawString(line, 8, 56);

    if (gps.altitudeValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "ALT   %.1f m", "高度  %.1f m"),
                      gps.altitudeMeters);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "ALT   --", "高度  --"));
    }
    canvas.drawString(line, 8, 70);

    if (gps.satellitesValid) {
        std::snprintf(line, sizeof(line), localized(language, "SAT   %lu", "卫星  %lu"),
                      static_cast<unsigned long>(gps.satellites));
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "SAT   --", "卫星  --"));
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
        std::snprintf(line, sizeof(line),
                      localized(language, "FIX AGE %.1fs", "定位 %.1fs前"),
                      gps.locationAgeMs / 1000.0);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "Waiting for position", "等待定位"));
    }
    canvas.drawString(line, 8, 101);
    if (gps.charsProcessed > 0) {
        std::snprintf(line, sizeof(line), localized(language, "DATA %.1fs", "数据 %.1fs前"),
                      gps.dataAgeMs / 1000.0);
        canvas.setTextDatum(top_right);
        canvas.drawString(line, config::kScreenWidth - 8, 101);
    }
}

void drawMotionAndTime(M5Canvas& canvas, const GpsSnapshot& gps,
                       UiLanguage language) {
    drawState(canvas, gps, language);
    char line[72];
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);

    if (gps.dateValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "UTC DATE  %04u-%02u-%02u",
                                "UTC 日期  %04u-%02u-%02u"),
                      gps.year, gps.month, gps.day);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "UTC DATE  --", "UTC 日期  --"));
    }
    canvas.drawString(line, 8, 42);

    if (gps.timeValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "UTC TIME  %02u:%02u:%02u.%02u",
                                "UTC 时间  %02u:%02u:%02u.%02u"),
                      gps.hour, gps.minute, gps.second, gps.centisecond);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "UTC TIME  --", "UTC 时间  --"));
    }
    canvas.drawString(line, 8, 56);

    if (gps.speedValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "SPEED     %.1f km/h", "速度      %.1f km/h"),
                      gps.speedKph);
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "SPEED     --", "速度      --"));
    }
    canvas.drawString(line, 8, 70);

    if (gps.courseValid) {
        std::snprintf(line, sizeof(line),
                      localized(language, "COURSE    %.1f deg %s", "航向      %.1f 度 %s"),
                      gps.courseDegrees,
                      localizedGpsCompassPoint(gps.courseDegrees, language));
    } else {
        std::snprintf(line, sizeof(line), "%s",
                      localized(language, "COURSE    --", "航向      --"));
    }
    canvas.drawString(line, 8, 84);

    canvas.setTextColor(theme::kMuted, theme::kBackground);
    std::snprintf(line, sizeof(line), localized(language, "MODE %s   QUALITY %s",
                                                "模式 %s   质量 %s"),
                  localizedGpsFixModeLabel(gps.fixMode, language),
                  localizedGpsFixQualityLabel(gps.fixQuality, language));
    canvas.drawString(line, 8, 101);
}

void drawReceiver(M5Canvas& canvas, const GpsSnapshot& gps, UiLanguage language) {
    drawState(canvas, gps, language);
    char line[64];
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString(localized(language, "UART 115200  RX15  TX13",
                                "串口 115200  RX15  TX13"),
                      8, 42);
    canvas.drawString("NMEA 0183 4.1 / TinyGPS+", 8, 56);

    std::snprintf(line, sizeof(line), localized(language, "RX CHARS       %lu", "接收字符  %lu"),
                  static_cast<unsigned long>(gps.charsProcessed));
    canvas.drawString(line, 8, 70);
    std::snprintf(line, sizeof(line), localized(language, "CHECKSUM OK    %lu", "校验通过  %lu"),
                  static_cast<unsigned long>(gps.sentencesPassed));
    canvas.drawString(line, 8, 84);
    std::snprintf(line, sizeof(line), localized(language, "CHECKSUM ERR   %lu", "校验错  %lu"),
                  static_cast<unsigned long>(gps.checksumFailed));
    canvas.drawString(line, 8, 98);

    std::snprintf(line, sizeof(line), localized(language, "FIX SENT %lu", "定位句 %lu"),
                  static_cast<unsigned long>(gps.sentencesWithFix));
    canvas.setTextDatum(top_right);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(line, config::kScreenWidth - 8, 98);
}

void drawMotion(M5Canvas& canvas, const GpsSnapshot& gps, UiLanguage language) {
    const bool speedAvailable =
        gps.speedValid && classifyGpsState(gps) == GpsState::Fix;
    const bool courseAvailable =
        gps.courseValid && classifyGpsState(gps) == GpsState::Fix;
    char line[24];

    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(localized(language, "SPEED", "速度"),
                      config::kScreenWidth / 2, 27);
    setTechnicalFont(canvas);
    canvas.setTextSize(3);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    if (speedAvailable) {
        std::snprintf(line, sizeof(line), "%.1f", gps.speedKph);
    } else {
        std::snprintf(line, sizeof(line), "--");
    }
    canvas.drawString(line, config::kScreenWidth / 2, 47);
    setTechnicalFont(canvas);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString("KM/H", config::kScreenWidth / 2, 65);

    setUiFont(canvas, language);
    canvas.drawString(localized(language, "COURSE", "航向"),
                      config::kScreenWidth / 2, 77);
    canvas.setTextColor(theme::kPrimary, theme::kBackground);
    if (courseAvailable) {
        if (isSimplifiedChinese(language)) {
            canvas.setTextSize(2);
            std::snprintf(line, sizeof(line), "%.0f 度 %s", gps.courseDegrees,
                          localizedGpsCompassPoint(gps.courseDegrees, language));
        } else {
            setTechnicalFont(canvas);
            canvas.setTextSize(3);
            std::snprintf(line, sizeof(line), "%.0f%s %s", gps.courseDegrees,
                          "\xC2\xB0", gpsCompassPoint(gps.courseDegrees));
        }
    } else {
        setTechnicalFont(canvas);
        canvas.setTextSize(3);
        std::snprintf(line, sizeof(line), "--");
    }
    canvas.drawString(line, config::kScreenWidth / 2, 97);
    setUiFont(canvas, language);
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
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    char title[24];
    std::snprintf(title, sizeof(title),
                  localized(language, "GPS %u/4", "GPS 定位 %u/4"),
                  static_cast<unsigned>(page) + 1);
    drawStatusBar(display, makeStatusBarData(title, context));
    auto& canvas = display.canvas();
    if (page == GpsPage::Position) {
        drawPosition(canvas, gps, language);
    } else if (page == GpsPage::Time) {
        drawMotionAndTime(canvas, gps, language);
    } else if (page == GpsPage::Receiver) {
        drawReceiver(canvas, gps, language);
    } else {
        drawMotion(canvas, gps, language);
    }
    drawHint(canvas, language);
}

}  // namespace pd
