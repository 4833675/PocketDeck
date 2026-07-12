#pragma once

#include <cstdint>

#include "core/input.h"

namespace pd {

enum class SettingsCategory : uint8_t {
    Bluetooth,
    System,
};

enum class SettingsPage : uint8_t {
    Categories,
    Bluetooth,
    System,
    Diagnostics,
    ConfirmForgetHost,
    ConfirmRestart,
    ConfirmFactoryReset,
};

enum class SettingsEffect : uint8_t {
    None,
    GoHome,
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
    SettingsResult handle(InputAction action);

    SettingsPage page() const { return page_; }
    SettingsCategory category() const { return category_; }
    uint8_t selectedRow() const { return selectedRow_; }

private:
    void moveRow(int direction, uint8_t rowCount);

    SettingsPage page_ = SettingsPage::Categories;
    SettingsCategory category_ = SettingsCategory::Bluetooth;
    uint8_t selectedRow_ = 0;
};

}  // namespace pd
