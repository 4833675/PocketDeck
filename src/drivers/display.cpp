#include "drivers/display.h"

#include "pocket_deck_config.h"
#include "ui/theme.h"

namespace pd {

Display::Display() : canvas_(&M5Cardputer.Display) {}

bool Display::begin() {
    static_assert(config::kDisplayColorDepth == 8,
                  "palette setup assumes an 8-bit back buffer");
    canvas_.setColorDepth(lgfx::color_depth_t::palette_8bit);
    if (canvas_.createSprite(config::kScreenWidth, config::kScreenHeight) == nullptr) return false;
    for (uint16_t index = 0; index < 256; ++index) {
        canvas_.setPaletteColor(index, theme::kBackground);
    }
    for (const uint16_t color : theme::kUiPalette) {
        canvas_.setPaletteColor(color & 0xFFu, color);
    }
    for (const uint16_t color : theme::kAnsiPalette) {
        canvas_.setPaletteColor(color & 0xFFu, color);
    }
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
