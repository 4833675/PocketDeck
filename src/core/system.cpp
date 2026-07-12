#include "core/system.h"

#include <Arduino.h>

#include "pocket_deck_config.h"

namespace pd {

System::System() : g0Gesture_(config::kG0LongPressMs, 25) {}

void System::begin() {
    Serial.begin(config::kSerialBaud);
    const bool detected = board_.begin();
    board_.setBrightness(config::kDefaultBrightness);
    board_.setVolume(config::kDefaultVolume);
    const bool canvasReady = display_.begin();
    const bool bleReady = bleKeyboard_.begin(config::kProductName);
    context_.bleKeyboard = &bleKeyboard_;
    context_.batteryPercent = board_.batteryPercent();
    const auto ble = bleKeyboard_.snapshot();
    context_.bleEnabled = ble.enabled;
    context_.bleConnected = ble.state == BleKeyboardState::Connected;
    current_ = &launcher_;
    current_->onEnter(context_);
    Serial.printf("Pocket Deck %s board=%d canvas=%d ble=%d\n", config::kFirmwareVersion,
                  detected ? 1 : 0, canvasReady ? 1 : 0, bleReady ? 1 : 0);
    render();
}

void System::update() {
    board_.update();
    const uint32_t nowMs = millis();
    context_.batteryPercent = board_.batteryPercent();
    bleKeyboard_.updateBattery(context_.batteryPercent);
    const auto ble = bleKeyboard_.snapshot();
    context_.bleEnabled = ble.enabled;
    context_.bleConnected = ble.state == BleKeyboardState::Connected;

    const G0Action g0 = g0Gesture_.update(board_.g0Down(), nowMs);
    if (g0 == G0Action::Home) goHome();

    const InputMode inputMode = current_->id() == AppId::Keyboard ? InputMode::Keyboard
                                                                  : InputMode::System;
    if (inputMode != InputMode::Keyboard || !context_.bleConnected) {
        context_.activeModifiers = 0;
    }
    const InputFrame frame = inputRouter_.update(board_.keyState(), inputMode,
                                                  context_.bleConnected);
    if (frame.hasHidReport && inputMode == InputMode::Keyboard) {
        context_.activeModifiers = frame.hidReport.modifier;
        bleKeyboard_.sendReport(frame.hidReport);
    }
    for (uint8_t i = 0; i < frame.eventCount; ++i) current_->onInput(frame.events[i], context_);
    current_->update(nowMs, context_);

    const AppId requested = context_.takeRequestedApp();
    if (requested != AppId::None) openApp(requested);

    if (nowMs - lastRenderMs_ >= 33) render();
    delay(1);
}

App* System::appForId(AppId id) {
    if (id == AppId::Launcher) return &launcher_;
    if (id == AppId::Keyboard) return &keyboard_;
    return nullptr;
}

void System::openApp(AppId id) {
    App* next = appForId(id);
    if (next == nullptr || next == current_) return;
    current_->onExit(context_);
    current_ = next;
    current_->onEnter(context_);
}

void System::goHome() {
    openApp(AppId::Launcher);
}

void System::render() {
    lastRenderMs_ = millis();
    display_.beginFrame();
    current_->render(display_, context_);
    display_.endFrame();
}

}  // namespace pd
