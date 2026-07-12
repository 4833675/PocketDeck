#include <Arduino.h>
#include <M5Cardputer.h>

#include "pocket_deck_config.h"

void setup() {
    Serial.begin(pd::config::kSerialBaud);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.drawString(pd::config::kProductName, 120, 67);

    Serial.println("Pocket Deck bootstrap");
}

void loop() {
    M5Cardputer.update();
    delay(5);
}

