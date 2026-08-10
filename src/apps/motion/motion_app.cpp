#include "apps/motion/motion_app.h"

#include <cstdio>

#include "core/localization.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/imu_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

constexpr float kBubbleLimitDegrees = 45.0f;

float clampBubbleAngle(float degrees) {
    if (degrees < -kBubbleLimitDegrees) return -kBubbleLimitDegrees;
    if (degrees > kBubbleLimitDegrees) return kBubbleLimitDegrees;
    return degrees;
}

const char* activityLabel(MotionActivity activity, UiLanguage language) {
    switch (activity) {
        case MotionActivity::Still: return localized(language, "STILL", "静止");
        case MotionActivity::Moving: return localized(language, "MOVING", "运动中");
        case MotionActivity::Shake: return localized(language, "SHAKE", "晃动");
    }
    return localized(language, "MOVING", "运动中");
}

uint16_t activityColor(MotionActivity activity) {
    switch (activity) {
        case MotionActivity::Still: return theme::kPrimary;
        case MotionActivity::Moving: return theme::kWarning;
        case MotionActivity::Shake: return theme::kError;
    }
    return theme::kMuted;
}

void drawHint(M5Canvas& canvas, MotionPage page, UiLanguage language) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    const char* hint = nullptr;
    switch (page) {
        case MotionPage::Live:
            hint = localized(language, "TAB/FN<> PAGE  BKSP/G0 HOME",
                             "TAB/FN<> 左右  BKSP/G0 返回");
            break;
        case MotionPage::Level:
            hint = localized(language, "ENTER ZERO TAB/FN<> BKSP/G0 HOME",
                             "ENTER 归零 TAB/FN<> BKSP/G0 返回");
            break;
        case MotionPage::Activity:
            hint = localized(language, "ENTER RESET TAB/FN<> BKSP/G0 HOME",
                             "ENTER清峰 TAB/FN翻页 BKSP/G0返回");
            break;
    }
    canvas.drawString(hint, config::kScreenWidth / 2,
                      y + theme::kHintHeight / 2);
}

void drawSensorMessage(M5Canvas& canvas, UiLanguage language,
                       const char* english, const char* chinese, uint16_t color) {
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(color, theme::kBackground);
    canvas.drawString(localized(language, english, chinese),
                      config::kScreenWidth / 2, 62);
}

void drawAxisColumn(M5Canvas& canvas, int16_t x, const char* heading,
                    const float values[3], bool gyro, UiLanguage language) {
    constexpr int16_t panelY = 22;
    constexpr int16_t panelW = 114;
    constexpr int16_t panelH = 90;
    canvas.fillRoundRect(x, panelY, panelW, panelH, 6, theme::kPanelRaised);
    canvas.drawRoundRect(x, panelY, panelW, panelH, 6, theme::kBorder);
    setUiFont(canvas, language);
    canvas.setTextDatum(top_center);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(heading, x + panelW / 2, panelY + 5);
    canvas.drawFastHLine(x + 6, panelY + 22, panelW - 12, theme::kBorder);

    constexpr char axes[3] = {'X', 'Y', 'Z'};
    char line[32]{};
    setTechnicalFont(canvas);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    for (uint8_t index = 0; index < 3; ++index) {
        if (gyro) {
            std::snprintf(line, sizeof(line), "%c %+6.1f deg/s", axes[index],
                          values[index]);
        } else {
            std::snprintf(line, sizeof(line), "%c %+6.2f g", axes[index],
                          values[index]);
        }
        canvas.drawString(line, x + 7, panelY + 29 + index * 17);
    }
}

void drawLive(M5Canvas& canvas, const ImuSnapshot& imu, UiLanguage language) {
    const float acceleration[3]{imu.ax, imu.ay, imu.az};
    const float gyro[3]{imu.gx, imu.gy, imu.gz};
    drawAxisColumn(canvas, 4, localized(language, "ACCEL (g)", "加速度 (g)"),
                   acceleration, false, language);
    drawAxisColumn(canvas, 122,
                   localized(language, "GYRO (deg/s)", "角速度 (deg/s)"), gyro,
                   true, language);
}

void drawLevel(M5Canvas& canvas, const ImuSnapshot& imu, UiLanguage language) {
    constexpr int16_t infoX = 4;
    constexpr int16_t infoY = 22;
    constexpr int16_t infoW = 87;
    constexpr int16_t infoH = 90;
    canvas.fillRoundRect(infoX, infoY, infoW, infoH, 6, theme::kPanelRaised);
    canvas.drawRoundRect(infoX, infoY, infoW, infoH, 6, theme::kBorder);

    char value[24]{};
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(localized(language, "ROLL", "横滚"), infoX + 7, infoY + 7);
    canvas.drawString(localized(language, "PITCH", "俯仰"), infoX + 7, infoY + 50);
    setTechnicalFont(canvas);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    std::snprintf(value, sizeof(value), "%+.1f deg", imu.rollDegrees);
    canvas.drawString(value, infoX + 7, infoY + 25);
    std::snprintf(value, sizeof(value), "%+.1f deg", imu.pitchDegrees);
    canvas.drawString(value, infoX + 7, infoY + 68);

    constexpr int16_t boxX = 99;
    constexpr int16_t boxY = 25;
    constexpr int16_t boxW = 136;
    constexpr int16_t boxH = 82;
    constexpr int16_t centerX = boxX + boxW / 2;
    constexpr int16_t centerY = boxY + boxH / 2;
    constexpr int16_t bubbleRadius = 6;
    constexpr int16_t maxXOffset = boxW / 2 - bubbleRadius - 3;
    constexpr int16_t maxYOffset = boxH / 2 - bubbleRadius - 3;
    canvas.fillRoundRect(boxX, boxY, boxW, boxH, 5, theme::kPanel);
    canvas.drawRoundRect(boxX, boxY, boxW, boxH, 5, theme::kBorder);
    canvas.drawFastHLine(boxX + 3, centerY, boxW - 6, theme::kBorder);
    canvas.drawFastVLine(centerX, boxY + 3, boxH - 6, theme::kBorder);

    const int16_t bubbleX = centerX + static_cast<int16_t>(
        clampBubbleAngle(imu.rollDegrees) * maxXOffset / kBubbleLimitDegrees);
    const int16_t bubbleY = centerY - static_cast<int16_t>(
        clampBubbleAngle(imu.pitchDegrees) * maxYOffset / kBubbleLimitDegrees);
    canvas.fillCircle(bubbleX, bubbleY, bubbleRadius, theme::kPrimary);
    canvas.drawCircle(bubbleX, bubbleY, bubbleRadius, theme::kText);
}

void drawActivity(M5Canvas& canvas, const ImuSnapshot& imu,
                  UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(activityColor(imu.activity), theme::kBackground);
    canvas.setTextSize(isSimplifiedChinese(language) ? 1 : 2);
    canvas.drawString(activityLabel(imu.activity, language),
                      config::kScreenWidth / 2, 32);

    char line[48]{};
    setUiFont(canvas, language);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    std::snprintf(line, sizeof(line), localized(language, "ACCEL   %.3f g",
                                                "加速度  %.3f g"),
                  imu.accelerationMagnitude);
    canvas.drawString(line, 24, 53);
    std::snprintf(line, sizeof(line), localized(language, "GYRO    %.1f deg/s",
                                                "角速度  %.1f deg/s"),
                  imu.gyroMagnitude);
    canvas.drawString(line, 24, 71);
    std::snprintf(line, sizeof(line), localized(language, "SESSION PEAK  %.3f g",
                                                "本次峰值  %.3f g"),
                  imu.peakAccelerationDeviation);
    canvas.setTextColor(theme::kSecondary, theme::kBackground);
    canvas.drawString(line, 24, 89);
}

}  // namespace

void MotionApp::onEnter(SystemContext&) {}
void MotionApp::onExit(SystemContext&) {}
void MotionApp::update(uint32_t, SystemContext&) {}

void MotionApp::onInput(const InputEvent& event, SystemContext& context) {
    const MotionAppResult result = model_.handle(event.action);
    switch (result.effect) {
        case MotionAppEffect::None: return;
        case MotionAppEffect::ZeroLevel:
            if (context.imu != nullptr) context.imu->zeroLevel();
            return;
        case MotionAppEffect::ResetPeak:
            if (context.imu != nullptr) context.imu->resetPeak();
            return;
        case MotionAppEffect::GoHome:
            context.requestApp(AppId::Launcher);
            return;
    }
}

void MotionApp::render(Display& display, const SystemContext& context) {
    const MotionPage page = model_.page();
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    char title[24]{};
    std::snprintf(title, sizeof(title), localized(language, "MOTION %u/3", "运动 %u/3"),
                  static_cast<unsigned>(page) + 1u);
    drawStatusBar(display, makeStatusBarData(title, context));

    auto& canvas = display.canvas();
    const ImuSnapshot imu = context.imu != nullptr ? context.imu->snapshot()
                                                   : ImuSnapshot{};
    if (!imu.available) {
        drawSensorMessage(canvas, language, "IMU UNAVAILABLE", "IMU 不可用",
                          theme::kError);
    } else if (!imu.hasSample) {
        drawSensorMessage(canvas, language, "WAITING FOR SAMPLE", "等待传感器数据",
                          theme::kWarning);
    } else if (!motionSampleIsCurrent(imu.hasSample, context.uptimeMs,
                                      imu.lastSampleMs)) {
        drawSensorMessage(canvas, language, "SENSOR DATA STALE", "传感器数据过期",
                          theme::kWarning);
    } else {
        switch (page) {
            case MotionPage::Live: drawLive(canvas, imu, language); break;
            case MotionPage::Level: drawLevel(canvas, imu, language); break;
            case MotionPage::Activity: drawActivity(canvas, imu, language); break;
        }
    }
    drawHint(canvas, page, language);
}

}  // namespace pd
