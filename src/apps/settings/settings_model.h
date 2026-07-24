#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class SettingsCategory : uint8_t {
    Wifi,
    Bluetooth,
    System,
};

enum class SettingsPage : uint8_t {
    Categories,
    Wifi,
    WifiNetworks,
    WifiPassword,
    WifiDiagnostics,
    Bluetooth,
    System,
    Diagnostics,
    ConfirmForgetWifi,
    ConfirmForgetHost,
    ConfirmRestart,
    ConfirmFactoryReset,
};

enum class SettingsEffect : uint8_t {
    None,
    GoHome,
    ToggleWifi,
    StartWifiScan,
    SelectWifiNetwork,
    ForgetWifi,
    ToggleBluetooth,
    DisconnectBluetooth,
    ForgetHost,
    Restart,
    FactoryReset,
};

struct SettingsResult {
    SettingsEffect effect = SettingsEffect::None;
};

class SettingsModel {
public:
    void reset();
    SettingsResult handle(InputAction action, uint8_t wifiNetworkCount = 0);
    void openWifiPassword();
    void cancelWifiPassword();
    void finishWifiConnection();

    SettingsPage page() const { return page_; }
    SettingsCategory category() const { return category_; }
    uint8_t selectedRow() const { return selectedRow_; }

private:
    void moveRow(int direction, uint8_t rowCount);
    void moveCategory(int direction);

    SettingsPage page_ = SettingsPage::Categories;
    SettingsCategory category_ = SettingsCategory::Wifi;
    uint8_t selectedRow_ = 0;
};

}  // namespace pd
