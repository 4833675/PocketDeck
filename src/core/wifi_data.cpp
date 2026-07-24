#include "core/wifi_data.h"

namespace pd {

const char* wifiStateLabel(WifiState state) {
    switch (state) {
        case WifiState::Disabled: return "OFF";
        case WifiState::Idle: return "IDLE";
        case WifiState::Scanning: return "SCANNING";
        case WifiState::Connecting: return "CONNECTING";
        case WifiState::Connected: return "CONNECTED";
        case WifiState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace pd
