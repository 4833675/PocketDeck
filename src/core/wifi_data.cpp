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

const char* wifiDisconnectReasonLabel(uint8_t reason) {
    switch (reason) {
        case 1: return "UNSPECIFIED";
        case 2: return "AUTH_EXPIRE";
        case 3: return "AUTH_LEAVE";
        case 4: return "ASSOC_EXPIRE";
        case 8: return "ASSOC_LEAVE";
        case 15: return "4WAY_TIMEOUT";
        case 34: return "MISSING_ACKS";
        case 39: return "TIMEOUT";
        case 46: return "PEER_INITIATED";
        case 47: return "AP_INITIATED";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        case 205: return "CONNECTION_FAIL";
        case 206: return "AP_TSF_RESET";
        case 207: return "ROAMING";
        default: return "OTHER";
    }
}

}  // namespace pd
