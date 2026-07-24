#include "services/wifi_service.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace pd {
namespace {

constexpr std::time_t kPlausibleEpoch = 1704067200;  // 2024-01-01 UTC

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const char* source) {
    destination.fill('\0');
    if (source != nullptr) std::strncpy(destination.data(), source, Size - 1);
}

template <std::size_t Size>
void copyIp(std::array<char, Size>& destination, const IPAddress& address) {
    const String text = address.toString();
    copyText(destination, text.c_str());
}

}  // namespace

bool WifiService::begin(bool enabled) {
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    setEnabled(enabled);
    return true;
}

void WifiService::setEnabled(bool enabled) {
    snapshot_.enabled = enabled;
    snapshot_.networkCount = 0;
    if (!enabled) {
        WiFi.scanDelete();
        WiFi.disconnect(true, false);
        clearLinkDetails();
        snapshot_.hasSavedNetwork = false;
        snapshot_.ntpSynced = false;
        snapshot_.utcEpoch = 0;
        timeRequested_ = false;
        wasConnected_ = false;
        setState(WifiState::Disabled, millis());
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    snapshot_.hasSavedNetwork = savedNetworkAvailable();
    loadSavedSsid();
    if (snapshot_.hasSavedNetwork) {
        WiFi.begin();
        setState(WifiState::Connecting, millis());
    } else {
        setState(WifiState::Idle, millis());
    }
}

bool WifiService::startScan() {
    if (!snapshot_.enabled) return false;
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    snapshot_.networkCount = 0;
    const int16_t result = WiFi.scanNetworks(true, true);
    if (result == WIFI_SCAN_FAILED) {
        setState(WifiState::Error, millis());
        return false;
    }
    setState(WifiState::Scanning, millis());
    return true;
}

bool WifiService::connect(const char* ssid, const char* password) {
    if (!snapshot_.enabled || ssid == nullptr || ssid[0] == '\0') return false;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.disconnect(false, false);
    delay(20);
    const wl_status_t result = WiFi.begin(ssid, password != nullptr ? password : "");
    snapshot_.lastStatus = static_cast<int16_t>(result);
    snapshot_.hasSavedNetwork = true;
    copyText(snapshot_.ssid, ssid);
    snapshot_.ntpSynced = false;
    snapshot_.utcEpoch = 0;
    timeRequested_ = false;
    setState(WifiState::Connecting, millis());
    return result != WL_CONNECT_FAILED;
}

bool WifiService::forgetNetwork() {
    const bool keepEnabled = snapshot_.enabled;
    if (!keepEnabled) WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    const bool result = WiFi.disconnect(false, true);
    snapshot_.hasSavedNetwork = false;
    snapshot_.networkCount = 0;
    snapshot_.ssid.fill('\0');
    snapshot_.ntpSynced = false;
    snapshot_.utcEpoch = 0;
    timeRequested_ = false;
    wasConnected_ = false;
    clearLinkDetails();
    if (keepEnabled) {
        setState(WifiState::Idle, millis());
    } else {
        WiFi.mode(WIFI_OFF);
        setState(WifiState::Disabled, millis());
    }
    return result;
}

void WifiService::update(uint32_t nowMs) {
    if (!snapshot_.enabled) return;

    snapshot_.lastStatus = static_cast<int16_t>(WiFi.status());
    if (snapshot_.state == WifiState::Scanning) {
        const int16_t result = WiFi.scanComplete();
        if (result >= 0) {
            collectScanResults(result);
            WiFi.scanDelete();
            setState(WiFi.status() == WL_CONNECTED ? WifiState::Connected : WifiState::Idle,
                     nowMs);
        } else if (result == WIFI_SCAN_FAILED) {
            setState(WifiState::Error, nowMs);
        }
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected) {
        if (!wasConnected_) {
            connectedAtMs_ = nowMs;
            requestTimeSync();
        }
        wasConnected_ = true;
        if (snapshot_.state != WifiState::Scanning) setState(WifiState::Connected, nowMs);
        refreshLinkDetails(nowMs);
        const std::time_t now = std::time(nullptr);
        if (now >= kPlausibleEpoch) {
            snapshot_.ntpSynced = true;
            snapshot_.utcEpoch = static_cast<int64_t>(now);
        }
        return;
    }

    if (wasConnected_) {
        wasConnected_ = false;
        connectedAtMs_ = 0;
        clearLinkDetails();
        timeRequested_ = false;
        if (snapshot_.hasSavedNetwork) {
            WiFi.reconnect();
            setState(WifiState::Connecting, nowMs);
        }
    }

    if (snapshot_.state == WifiState::Connecting &&
        nowMs - stateSinceMs_ >= kConnectTimeoutMs) {
        setState(WifiState::Error, nowMs);
    }
}

bool WifiService::savedNetworkAvailable() const {
    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) return false;
    return config.sta.ssid[0] != '\0';
}

void WifiService::loadSavedSsid() {
    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) {
        snapshot_.ssid.fill('\0');
        return;
    }
    copyText(snapshot_.ssid, reinterpret_cast<const char*>(config.sta.ssid));
}

void WifiService::collectScanResults(int16_t count) {
    snapshot_.networkCount = 0;
    for (int16_t index = 0; index < count; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) continue;

        int existing = -1;
        for (uint8_t saved = 0; saved < snapshot_.networkCount; ++saved) {
            if (std::strcmp(snapshot_.networks[saved].ssid.data(), ssid.c_str()) == 0) {
                existing = saved;
                break;
            }
        }

        const int32_t rssi = WiFi.RSSI(index);
        if (existing >= 0) {
            if (rssi <= snapshot_.networks[existing].rssi) continue;
        } else {
            if (snapshot_.networkCount == WifiSnapshot::kNetworkCapacity) continue;
            existing = snapshot_.networkCount++;
        }

        WifiNetwork& network = snapshot_.networks[existing];
        copyText(network.ssid, ssid.c_str());
        network.rssi = rssi;
        network.channel = static_cast<uint8_t>(WiFi.channel(index));
        network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }

    std::sort(snapshot_.networks.begin(),
              snapshot_.networks.begin() + snapshot_.networkCount,
              [](const WifiNetwork& left, const WifiNetwork& right) {
                  return left.rssi > right.rssi;
              });
}

void WifiService::refreshLinkDetails(uint32_t nowMs) {
    snapshot_.connected = true;
    snapshot_.hasSavedNetwork = true;
    snapshot_.rssi = WiFi.RSSI();
    snapshot_.connectedForMs = connectedAtMs_ == 0 ? 0 : nowMs - connectedAtMs_;
    const String ssid = WiFi.SSID();
    copyText(snapshot_.ssid, ssid.c_str());
    copyIp(snapshot_.ip, WiFi.localIP());
    copyIp(snapshot_.gateway, WiFi.gatewayIP());
    copyIp(snapshot_.dns, WiFi.dnsIP());
}

void WifiService::setState(WifiState state, uint32_t nowMs) {
    if (snapshot_.state == state) return;
    snapshot_.state = state;
    stateSinceMs_ = nowMs;
}

void WifiService::requestTimeSync() {
    if (timeRequested_) return;
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
    timeRequested_ = true;
}

void WifiService::clearLinkDetails() {
    snapshot_.connected = false;
    snapshot_.rssi = -127;
    snapshot_.connectedForMs = 0;
    snapshot_.ip.fill('\0');
    snapshot_.gateway.fill('\0');
    snapshot_.dns.fill('\0');
}

}  // namespace pd
