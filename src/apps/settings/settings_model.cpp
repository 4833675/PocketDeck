#include "apps/settings/settings_model.h"

namespace pd {

void SettingsModel::reset() {
    page_ = SettingsPage::Categories;
    category_ = SettingsCategory::Bluetooth;
    selectedRow_ = 0;
}

SettingsResult SettingsModel::handle(InputAction action) {
    if (page_ == SettingsPage::Categories) {
        if (action == InputAction::Up || action == InputAction::Down) {
            category_ = category_ == SettingsCategory::Bluetooth ? SettingsCategory::System
                                                                 : SettingsCategory::Bluetooth;
        } else if (action == InputAction::Confirm) {
            page_ = category_ == SettingsCategory::Bluetooth ? SettingsPage::Bluetooth
                                                              : SettingsPage::System;
            selectedRow_ = 0;
        } else if (action == InputAction::Back) {
            return {SettingsEffect::GoHome};
        }
        return {};
    }

    if (page_ == SettingsPage::Diagnostics) {
        if (action == InputAction::Back) page_ = SettingsPage::System;
        return {};
    }

    if (page_ == SettingsPage::ConfirmForgetHost || page_ == SettingsPage::ConfirmRestart ||
        page_ == SettingsPage::ConfirmFactoryReset) {
        if (action == InputAction::Back) {
            page_ = page_ == SettingsPage::ConfirmForgetHost ? SettingsPage::Bluetooth
                                                              : SettingsPage::System;
            return {};
        }
        if (action != InputAction::Confirm) return {};

        if (page_ == SettingsPage::ConfirmForgetHost) {
            page_ = SettingsPage::Bluetooth;
            return {SettingsEffect::ForgetHost};
        }
        if (page_ == SettingsPage::ConfirmRestart) {
            page_ = SettingsPage::System;
            return {SettingsEffect::Restart};
        }
        page_ = SettingsPage::System;
        return {SettingsEffect::FactoryReset};
    }

    if (action == InputAction::Back) {
        page_ = SettingsPage::Categories;
        selectedRow_ = 0;
        return {};
    }

    if (action == InputAction::Up) {
        moveRow(-1, 3);
        return {};
    }
    if (action == InputAction::Down) {
        moveRow(1, 3);
        return {};
    }
    if (action != InputAction::Confirm) return {};

    if (page_ == SettingsPage::Bluetooth) {
        if (selectedRow_ == 0) return {SettingsEffect::ToggleBluetooth};
        if (selectedRow_ == 1) return {SettingsEffect::DisconnectBluetooth};
        page_ = SettingsPage::ConfirmForgetHost;
        return {};
    }

    if (selectedRow_ == 0) {
        page_ = SettingsPage::Diagnostics;
    } else if (selectedRow_ == 1) {
        page_ = SettingsPage::ConfirmRestart;
    } else {
        page_ = SettingsPage::ConfirmFactoryReset;
    }
    return {};
}

void SettingsModel::moveRow(int direction, uint8_t rowCount) {
    const int next = static_cast<int>(selectedRow_) + direction + rowCount;
    selectedRow_ = static_cast<uint8_t>(next % rowCount);
}

}  // namespace pd
