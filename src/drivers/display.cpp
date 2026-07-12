#include "drivers/display.h"

#include "pocket_deck_config.h"
#include "ui/theme.h"

namespace pd {

Display::Display() : canvas_(&M5Cardputer.Display) {}

bool Display::begin() {
    canvas_.setColorDepth(16);
    if (canvas_.createSprite(config::kScreenWidth, config::kScreenHeight) == nullptr) return false;
    canvas_.setTextFont(1);
    canvas_.setTextWrap(false);
    return true;
}

void Display::beginFrame() {
    canvas_.fillSprite(theme::kBackground);
    canvas_.setTextFont(1);
    canvas_.setTextSize(1);
    canvas_.setTextColor(theme::kText, theme::kBackground);
}

void Display::endFrame() {
    canvas_.pushSprite(0, 0);
}

}  // namespace pd

