#include "core/system.h"

#include <ctime>

#include <Arduino.h>
#include <esp_system.h>

#include "core/clock_data.h"
#include "pocket_deck_config.h"

namespace pd {
namespace {

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt-wdt";
        case ESP_RST_TASK_WDT: return "task-wdt";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_UNKNOWN: return "unknown";
    }
    return "other";
}

const char* bleStateName(BleKeyboardState state) {
    switch (state) {
        case BleKeyboardState::Disabled: return "disabled";
        case BleKeyboardState::Advertising: return "advertising";
        case BleKeyboardState::Pairing: return "pairing";
        case BleKeyboardState::Connected: return "connected";
        case BleKeyboardState::Error: return "error";
    }
    return "unknown";
}

}  // namespace

System::System() : g0Gesture_(config::kG0LongPressMs, 25) {}

void System::begin() {
    Serial.begin(config::kSerialBaud);
    context_.resetReason = resetReasonName(esp_reset_reason());
    context_.bleKeyboard = &bleKeyboard_;
    context_.gps = &gps_;
    context_.wifi = &wifi_;
    context_.weather = &weather_;
    context_.sdLog = &sdLog_;
    context_.diagnostics = &diagnostics_;
    context_.settings = &settings_;
    diagnostics_.logf("Boot reset: %s", context_.resetReason);

    const SettingsLoadResult loaded = settingsStore_.load();
    settings_ = loaded.settings;
    if (!loaded.storageReady) {
        diagnostics_.log("Settings NVS unavailable; defaults");
    } else if (loaded.found && !loaded.valid) {
        diagnostics_.log("Invalid settings; defaults restored");
    } else if (!loaded.found) {
        diagnostics_.log("Settings defaults loaded");
    }

    const bool detected = board_.begin();
    diagnostics_.logf("Board init: Cardputer ADV=%d", detected ? 1 : 0);
    board_.setBrightness(settings_.brightness);
    board_.setVolume(settings_.volume);
    const bool sdReady = sdLog_.begin();
    if (sdReady) sdLog_.beginSession(config::kFirmwareVersion, context_.resetReason);
    diagnostics_.setSink(&SdLogService::diagnosticsSink, &sdLog_, true);
    diagnostics_.logf("TF logging: %s", sdReady ? "ready /PocketDeck/ble.log"
                                                : "unavailable");
    const bool canvasReady = display_.begin();
    const bool gpsReady = gps_.begin();
    diagnostics_.logf("GPS UART: %s 115200 RX15 TX13", gpsReady ? "ready" : "failed");
    bleKeyboard_.setDiagnostics(&diagnostics_);
    bleKeyboard_.setEnabled(settings_.bleEnabled);
    const bool bleReady = bleKeyboard_.begin(settings_.deviceName.data());
    const bool wifiReady = wifi_.begin(settings_.wifiEnabled);
    const bool weatherReady = weather_.begin();
    refreshContext(millis());
    trackBleState(bleKeyboard_.snapshot());
    trackWifiState(wifi_.snapshot());
    trackWeatherState(weather_.snapshot());
    current_ = &launcher_;
    current_->onEnter(context_);
    diagnostics_.logf("System ready: DISP=%d BLE=%d GPS=%d WIFI=%d WX=%d",
                      canvasReady ? 1 : 0, bleReady ? 1 : 0, gpsReady ? 1 : 0,
                      wifiReady ? 1 : 0, weatherReady ? 1 : 0);
    render();
}

void System::update() {
    board_.update();
    const uint32_t nowMs = millis();
    gps_.update();
    wifi_.update(nowMs);
    bleKeyboard_.update(nowMs);
    diagnostics_.drainPending();
    refreshContext(nowMs);
    bleKeyboard_.updateBattery(context_.batteryPercent);
    trackBleState(bleKeyboard_.snapshot());
    trackGpsState(gps_.snapshot());
    trackWifiState(wifi_.snapshot());
    trackWeatherState(weather_.snapshot());

    const G0Action g0 = g0Gesture_.update(board_.g0Down(), nowMs);
    if (g0 == G0Action::QuickSettings && !quickSettings_.active()) {
        openQuickSettings();
    } else if (g0 == G0Action::Home) {
        if (quickSettings_.active()) {
            closeQuickSettings();
        } else {
            goHome();
        }
    }

    const InputMode inputMode = quickSettings_.active() ? InputMode::System
                                                        : current_->inputMode();
    if (inputMode != InputMode::Keyboard || !context_.bleConnected) {
        context_.activeModifiers = 0;
    }
    const InputFrame frame = inputRouter_.update(board_.keyState(), inputMode,
                                                  context_.bleConnected);
    if (quickSettings_.active()) {
        for (uint8_t i = 0; i < frame.eventCount; ++i) {
            handleQuickSettingsResult(quickSettings_.handle(frame.events[i].action));
            if (!quickSettings_.active()) break;
        }
    } else if (frame.hasHidReport && inputMode == InputMode::Keyboard) {
        context_.activeModifiers = frame.hidReport.modifier;
        bleKeyboard_.sendReport(frame.hidReport);
    } else {
        for (uint8_t i = 0; i < frame.eventCount; ++i) {
            current_->onInput(frame.events[i], context_);
        }
    }
    current_->update(nowMs, context_);

    handleSystemCommand(context_.takeRequestedCommand());

    const AppId requested = context_.takeRequestedApp();
    if (requested != AppId::None) openApp(requested);

    if (nowMs - lastRenderMs_ >= 33) render();
    delay(1);
}

App* System::appForId(AppId id) {
    if (id == AppId::Launcher) return &launcher_;
    if (id == AppId::Keyboard) return &keyboard_;
    if (id == AppId::Gps) return &gpsApp_;
    if (id == AppId::Weather) return &weatherApp_;
    if (id == AppId::Settings) return &settingsApp_;
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

void System::openQuickSettings() {
    if (quickSettings_.active()) return;
    if (current_->id() == AppId::Keyboard) bleKeyboard_.sendReport(HidReport{});
    context_.activeModifiers = 0;
    quickSettings_.open({settings_.brightness, settings_.volume, settings_.bleEnabled});
}

void System::closeQuickSettings() {
    handleQuickSettingsResult(quickSettings_.close());
}

void System::handleQuickSettingsResult(const QuickSettingsResult& result) {
    if (result.valuesChanged) {
        const QuickSettingsValues values = quickSettings_.values();
        const bool bleChanged = settings_.bleEnabled != values.bleEnabled;
        settings_.brightness = values.brightness;
        settings_.volume = values.volume;
        settings_.bleEnabled = values.bleEnabled;
        board_.setBrightness(settings_.brightness);
        board_.setVolume(settings_.volume);
        if (bleChanged) bleKeyboard_.setEnabled(settings_.bleEnabled);
    }
    if (result.closed && result.persist) saveSettings();
}

void System::handleSystemCommand(SystemCommand command) {
    switch (command) {
        case SystemCommand::None: return;
        case SystemCommand::ToggleWifi:
            settings_.wifiEnabled = !settings_.wifiEnabled;
            wifi_.setEnabled(settings_.wifiEnabled);
            diagnostics_.log(settings_.wifiEnabled ? "WiFi enabled" : "WiFi disabled");
            saveSettings();
            return;
        case SystemCommand::StartWifiScan:
            diagnostics_.log(wifi_.startScan() ? "WiFi scan started" : "WiFi scan failed");
            return;
        case SystemCommand::ConnectWifi: {
            WifiConnectRequest request;
            if (!context_.takeWifiConnectRequest(request)) return;
            const bool started = wifi_.connect(request.ssid.data(), request.password.data());
            request.password.fill('\0');
            diagnostics_.log(started ? "WiFi connection started" : "WiFi connection failed");
            return;
        }
        case SystemCommand::ForgetWifi:
            diagnostics_.log(wifi_.forgetNetwork() ? "WiFi network forgotten"
                                                   : "WiFi forget failed");
            return;
        case SystemCommand::ToggleBluetooth:
            settings_.bleEnabled = !settings_.bleEnabled;
            bleKeyboard_.setEnabled(settings_.bleEnabled);
            diagnostics_.log(settings_.bleEnabled ? "Bluetooth enabled" : "Bluetooth disabled");
            saveSettings();
            return;
        case SystemCommand::DisconnectBluetooth:
            bleKeyboard_.disconnect();
            diagnostics_.log("Bluetooth disconnect requested");
            return;
        case SystemCommand::ForgetHost: {
            const bool forgotten = bleKeyboard_.forgetHost();
            diagnostics_.log(forgotten ? "BLE host bond deleted" : "BLE bond deletion failed");
            return;
        }
        case SystemCommand::MountStorage: {
            const bool mounted = sdLog_.remount();
            if (mounted) sdLog_.beginSession(config::kFirmwareVersion, context_.resetReason);
            diagnostics_.log(mounted ? "TF card mounted; logging active"
                                     : "TF card mount failed");
            return;
        }
        case SystemCommand::FormatStorage: {
            diagnostics_.log("TF card format confirmed");
            const bool formatted = sdLog_.formatCard();
            if (formatted) sdLog_.beginSession(config::kFirmwareVersion, context_.resetReason);
            diagnostics_.log(formatted ? "TF card formatted; logging active"
                                       : "TF card format failed");
            return;
        }
        case SystemCommand::Restart:
            diagnostics_.log("System restart requested");
            delay(80);
            ESP.restart();
            return;
        case SystemCommand::FactoryReset: {
            diagnostics_.log("Factory reset confirmed");
            const bool settingsCleared = settingsStore_.clear();
            const bool wifiCleared = wifi_.forgetNetwork();
            const bool bondCleared = bleKeyboard_.forgetHost();
            diagnostics_.logf("Factory clear: settings=%d wifi=%d bond=%d",
                              settingsCleared ? 1 : 0, wifiCleared ? 1 : 0,
                              bondCleared ? 1 : 0);
            delay(100);
            ESP.restart();
            return;
        }
    }
}

void System::refreshContext(uint32_t nowMs) {
    context_.uptimeMs = nowMs;
    context_.batteryPercent = board_.batteryPercent();
    context_.freeHeap = ESP.getFreeHeap();
    context_.minimumFreeHeap = ESP.getMinFreeHeap();
    const BleKeyboardSnapshot ble = bleKeyboard_.snapshot();
    context_.bleEnabled = ble.enabled;
    context_.bleConnected = ble.state == BleKeyboardState::Connected;
    const WifiSnapshot& wifi = wifi_.snapshot();
    context_.wifiEnabled = wifi.enabled;
    context_.wifiConnected = wifi.connected;
    const ClockDisplay localClock = clockFromUtcEpoch(
        static_cast<int64_t>(std::time(nullptr)), config::kLocalUtcOffsetSeconds);
    context_.clockValid = localClock.valid;
    context_.clockHour = localClock.hour;
    context_.clockMinute = localClock.minute;
}

void System::trackBleState(const BleKeyboardSnapshot& snapshot) {
    if (!lastBleSnapshotValid_ || snapshot.state != lastBleSnapshot_.state) {
        diagnostics_.logf("BLE state: %s", bleStateName(snapshot.state));
    }
    if (lastBleSnapshotValid_ && lastBleSnapshot_.connected && !snapshot.connected) {
        diagnostics_.logf("BLE disconnected: 0x%02x", snapshot.lastDisconnectReason);
    }
    if ((!lastBleSnapshotValid_ || !lastBleSnapshot_.encrypted) && snapshot.encrypted) {
        diagnostics_.log("BLE authentication succeeded");
    }
    if (lastBleSnapshotValid_ && snapshot.error != lastBleSnapshot_.error &&
        snapshot.error != BleKeyboardError::None) {
        diagnostics_.logf("BLE error: %s", bleKeyboard_.errorText());
    }
    lastBleSnapshot_ = snapshot;
    lastBleSnapshotValid_ = true;
}

void System::trackGpsState(const GpsSnapshot& snapshot) {
    const GpsState state = classifyGpsState(snapshot);
    if (!lastGpsStateValid_ || state != lastGpsState_) {
        diagnostics_.logf("GPS state: %s", gpsStateLabel(state));
        lastGpsState_ = state;
        lastGpsStateValid_ = true;
    }
}

void System::trackWifiState(const WifiSnapshot& snapshot) {
    if (!lastWifiStateValid_ || snapshot.state != lastWifiState_) {
        diagnostics_.logf("WiFi state: %s", wifiStateLabel(snapshot.state));
        lastWifiState_ = snapshot.state;
        lastWifiStateValid_ = true;
    }
    if (snapshot.ntpSynced && snapshot.utcEpoch > 0) {
        static bool loggedTimeSync = false;
        if (!loggedTimeSync) {
            diagnostics_.log("NTP time synchronized");
            loggedTimeSync = true;
        }
    }
}

void System::trackWeatherState(const WeatherSnapshot& snapshot) {
    if (!lastWeatherStateValid_ || snapshot.state != lastWeatherState_) {
        diagnostics_.logf("Weather state: %s", weatherStateLabel(snapshot.state));
        if (snapshot.state == WeatherState::Error && snapshot.error[0] != '\0') {
            diagnostics_.logf("Weather error: %.30s", snapshot.error.data());
        }
        lastWeatherState_ = snapshot.state;
        lastWeatherStateValid_ = true;
    }
}

bool System::saveSettings() {
    settings_ = sanitizeSettings(settings_);
    if (settingsStore_.save(settings_)) return true;
    diagnostics_.log("Settings save failed");
    return false;
}

void System::render() {
    lastRenderMs_ = millis();
    display_.beginFrame();
    current_->render(display_, context_);
    if (quickSettings_.active()) quickSettings_.render(display_, context_.batteryPercent);
    display_.endFrame();
}

}  // namespace pd
