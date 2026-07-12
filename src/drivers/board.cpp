#include "drivers/board.h"

#include <Arduino.h>
#include <M5Cardputer.h>

namespace pd {

bool Board::begin() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Speaker.begin();
    pinMode(0, INPUT_PULLUP);
    return M5.getBoard() == m5::board_t::board_M5CardputerADV;
}

void Board::update() {
    M5Cardputer.update();
}

KeyState Board::keyState() const {
    KeyState state;
    const auto& pressed = M5Cardputer.Keyboard.keyList();
    for (const auto& point : pressed) {
        if (state.count == KeyState::kCapacity) break;
        const auto key = physicalKeyFromMatrix(static_cast<uint8_t>(point.y),
                                               static_cast<uint8_t>(point.x));
        if (key != PhysicalKey::None) state.keys[state.count++] = key;
    }
    return state;
}

bool Board::g0Down() const {
    return digitalRead(0) == LOW;
}

uint8_t Board::batteryPercent() const {
    const int level = M5.Power.getBatteryLevel();
    if (level < 0) return 0;
    if (level > 100) return 100;
    return static_cast<uint8_t>(level);
}

void Board::setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    M5Cardputer.Display.setBrightness(static_cast<uint8_t>((percent * 255u) / 100u));
}

void Board::setVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    M5Cardputer.Speaker.setVolume(static_cast<uint8_t>((percent * 255u) / 100u));
}

}  // namespace pd

