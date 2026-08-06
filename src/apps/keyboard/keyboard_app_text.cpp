#include "apps/keyboard/keyboard_app_text.h"

namespace pd {

const char* localizedKeyboardStateLabel(BleKeyboardState state,
                                        UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (state) {
            case BleKeyboardState::Disabled: return "蓝牙已关闭";
            case BleKeyboardState::Advertising: return "等待 Mac";
            case BleKeyboardState::Pairing: return "正在配对";
            case BleKeyboardState::Connected: return "已连接";
            case BleKeyboardState::Error: return "错误";
        }
        return "未知";
    }
    switch (state) {
        case BleKeyboardState::Disabled: return "BLUETOOTH OFF";
        case BleKeyboardState::Advertising: return "WAITING FOR MAC";
        case BleKeyboardState::Pairing: return "PAIRING";
        case BleKeyboardState::Connected: return "CONNECTED";
        case BleKeyboardState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* localizedKeyboardErrorLabel(BleKeyboardError error,
                                        UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (error) {
            case BleKeyboardError::None: return "无";
            case BleKeyboardError::InitializationFailed: return "初始化失败";
            case BleKeyboardError::UnauthorizedPeer: return "已拒绝未知主机";
            case BleKeyboardError::AuthenticationFailed: return "认证失败";
            case BleKeyboardError::BondOperationFailed: return "配对记录操作失败";
        }
        return "未知";
    }
    switch (error) {
        case BleKeyboardError::None: return "none";
        case BleKeyboardError::InitializationFailed: return "initialization failed";
        case BleKeyboardError::UnauthorizedPeer: return "unknown host rejected";
        case BleKeyboardError::AuthenticationFailed: return "authentication failed";
        case BleKeyboardError::BondOperationFailed: return "bond operation failed";
    }
    return "unknown";
}

}  // namespace pd
