#include "apps/settings/settings_model.h"

namespace pd {

void SettingsModel::reset() {
    page_ = SettingsPage::Categories;
    category_ = SettingsCategory::Wifi;
    selectedRow_ = 0;
}

SettingsResult SettingsModel::handle(InputAction action, uint8_t wifiNetworkCount,
                                     uint8_t wifiSavedNetworkCount) {
    if (page_ == SettingsPage::Categories) {
        if (action == InputAction::Up) {
            moveCategory(-1);
        } else if (action == InputAction::Down) {
            moveCategory(1);
        } else if (action == InputAction::Confirm) {
            if (category_ == SettingsCategory::Wifi) {
                page_ = SettingsPage::Wifi;
            } else if (category_ == SettingsCategory::Bluetooth) {
                page_ = SettingsPage::Bluetooth;
            } else {
                page_ = SettingsPage::System;
            }
            selectedRow_ = 0;
        } else if (action == InputAction::Back) {
            return {SettingsEffect::GoHome};
        }
        return {};
    }

    if (page_ == SettingsPage::WifiNetworks) {
        if (action == InputAction::Back) {
            page_ = SettingsPage::Wifi;
            selectedRow_ = 1;
        } else if (action == InputAction::Up && wifiNetworkCount > 0) {
            moveRow(-1, wifiNetworkCount);
        } else if (action == InputAction::Down && wifiNetworkCount > 0) {
            moveRow(1, wifiNetworkCount);
        } else if (action == InputAction::Confirm && wifiNetworkCount > 0) {
            if (selectedRow_ >= wifiNetworkCount) selectedRow_ = 0;
            return {SettingsEffect::SelectWifiNetwork};
        } else if (action == InputAction::Tab) {
            return {SettingsEffect::StartWifiScan};
        }
        return {};
    }

    if (page_ == SettingsPage::WifiSavedNetworks) {
        if (action == InputAction::Back) {
            page_ = SettingsPage::Wifi;
            selectedRow_ = 2;
        } else if (action == InputAction::Up && wifiSavedNetworkCount > 0) {
            moveRow(-1, wifiSavedNetworkCount);
        } else if (action == InputAction::Down && wifiSavedNetworkCount > 0) {
            moveRow(1, wifiSavedNetworkCount);
        } else if (action == InputAction::Confirm && wifiSavedNetworkCount > 0) {
            if (selectedRow_ >= wifiSavedNetworkCount) selectedRow_ = 0;
            page_ = SettingsPage::ConfirmForgetWifi;
            return {SettingsEffect::SelectWifiForForget};
        }
        return {};
    }

    if (page_ == SettingsPage::WifiPassword) {
        if (action == InputAction::Back) cancelWifiPassword();
        return {};
    }

    if (page_ == SettingsPage::WifiDiagnostics) {
        if (action == InputAction::Back) {
            page_ = SettingsPage::Wifi;
            selectedRow_ = 3;
        }
        return {};
    }

    if (page_ == SettingsPage::Diagnostics) {
        if (action == InputAction::Back) {
            page_ = SettingsPage::System;
            selectedRow_ = 1;
        }
        return {};
    }

    if (page_ == SettingsPage::Storage) {
        if (action == InputAction::Back) {
            page_ = SettingsPage::System;
            selectedRow_ = 2;
        } else if (action == InputAction::Up) {
            moveRow(-1, 2);
        } else if (action == InputAction::Down) {
            moveRow(1, 2);
        } else if (action == InputAction::Confirm) {
            if (selectedRow_ == 0) return {SettingsEffect::MountStorage};
            page_ = SettingsPage::ConfirmFormatStorage;
        }
        return {};
    }

    if (page_ == SettingsPage::ConfirmForgetWifi ||
        page_ == SettingsPage::ConfirmForgetHost || page_ == SettingsPage::ConfirmRestart ||
        page_ == SettingsPage::ConfirmFormatStorage ||
        page_ == SettingsPage::ConfirmFactoryReset) {
        if (action == InputAction::Back) {
            if (page_ == SettingsPage::ConfirmForgetWifi) {
                page_ = SettingsPage::WifiSavedNetworks;
            } else if (page_ == SettingsPage::ConfirmForgetHost) {
                page_ = SettingsPage::Bluetooth;
            } else if (page_ == SettingsPage::ConfirmFormatStorage) {
                page_ = SettingsPage::Storage;
                selectedRow_ = 1;
            } else {
                page_ = SettingsPage::System;
            }
            return {};
        }
        if (action != InputAction::Confirm) return {};

        if (page_ == SettingsPage::ConfirmForgetWifi) {
            page_ = SettingsPage::WifiSavedNetworks;
            if (wifiSavedNetworkCount > 1 && selectedRow_ >= wifiSavedNetworkCount - 1) {
                --selectedRow_;
            }
            return {SettingsEffect::ForgetWifi};
        }
        if (page_ == SettingsPage::ConfirmForgetHost) {
            page_ = SettingsPage::Bluetooth;
            return {SettingsEffect::ForgetHost};
        }
        if (page_ == SettingsPage::ConfirmRestart) {
            page_ = SettingsPage::System;
            selectedRow_ = 3;
            return {SettingsEffect::Restart};
        }
        if (page_ == SettingsPage::ConfirmFormatStorage) {
            page_ = SettingsPage::Storage;
            selectedRow_ = 1;
            return {SettingsEffect::FormatStorage};
        }
        page_ = SettingsPage::System;
        selectedRow_ = 4;
        return {SettingsEffect::FactoryReset};
    }

    if (action == InputAction::Back) {
        page_ = SettingsPage::Categories;
        selectedRow_ = 0;
        return {};
    }

    uint8_t rowCount = 3;
    if (page_ == SettingsPage::Wifi) rowCount = 4;
    if (page_ == SettingsPage::System) rowCount = 5;
    if (action == InputAction::Up) {
        moveRow(-1, rowCount);
        return {};
    }
    if (action == InputAction::Down) {
        moveRow(1, rowCount);
        return {};
    }
    if (action != InputAction::Confirm) return {};

    if (page_ == SettingsPage::Wifi) {
        if (selectedRow_ == 0) return {SettingsEffect::ToggleWifi};
        if (selectedRow_ == 1) {
            page_ = SettingsPage::WifiNetworks;
            selectedRow_ = 0;
            return {SettingsEffect::StartWifiScan};
        }
        if (selectedRow_ == 2) {
            page_ = SettingsPage::WifiSavedNetworks;
            selectedRow_ = 0;
            return {};
        }
        if (selectedRow_ == 3) {
            page_ = SettingsPage::WifiDiagnostics;
            return {};
        }
    }

    if (page_ == SettingsPage::Bluetooth) {
        if (selectedRow_ == 0) return {SettingsEffect::ToggleBluetooth};
        if (selectedRow_ == 1) return {SettingsEffect::DisconnectBluetooth};
        page_ = SettingsPage::ConfirmForgetHost;
        return {};
    }

    if (selectedRow_ == 0) {
        return {SettingsEffect::ToggleLanguage};
    } else if (selectedRow_ == 1) {
        page_ = SettingsPage::Diagnostics;
    } else if (selectedRow_ == 2) {
        page_ = SettingsPage::Storage;
        selectedRow_ = 0;
    } else if (selectedRow_ == 3) {
        page_ = SettingsPage::ConfirmRestart;
    } else {
        page_ = SettingsPage::ConfirmFactoryReset;
    }
    return {};
}

void SettingsModel::openWifiPassword() {
    page_ = SettingsPage::WifiPassword;
}

void SettingsModel::cancelWifiPassword() {
    page_ = SettingsPage::WifiNetworks;
}

void SettingsModel::finishWifiConnection() {
    page_ = SettingsPage::Wifi;
    selectedRow_ = 1;
}

void SettingsModel::moveRow(int direction, uint8_t rowCount) {
    if (rowCount == 0) return;
    const int next = static_cast<int>(selectedRow_) + direction + rowCount;
    selectedRow_ = static_cast<uint8_t>(next % rowCount);
}

void SettingsModel::moveCategory(int direction) {
    constexpr int count = 3;
    const int current = static_cast<int>(category_);
    category_ = static_cast<SettingsCategory>((current + direction + count) % count);
}

}  // namespace pd
