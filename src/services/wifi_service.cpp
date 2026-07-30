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

bool sameText(const char* left, const char* right) {
    return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
}

bool clearLegacyStationConfig() {
    wifi_config_t empty{};
    const bool selectedFlash = esp_wifi_set_storage(WIFI_STORAGE_FLASH) == ESP_OK;
    const bool cleared = selectedFlash &&
                         esp_wifi_set_config(WIFI_IF_STA, &empty) == ESP_OK;
    const bool restoredRam = esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK;
    return cleared && restoredRam;
}

}  // namespace

bool WifiService::begin(bool enabled) {
    const WifiProfilesLoadResult loaded = profileStore_.load();
    if (loaded.valid) profiles_ = loaded.profiles;
    syncSavedNetworks();

    // Pocket Deck owns all reconnect decisions. Disabling Arduino's persistent
    // single-station config prevents an unsuccessful attempt from overwriting
    // one of our known-good profiles.
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    setEnabled(enabled);
    return true;
}

void WifiService::setEnabled(bool enabled) {
    snapshot_.enabled = enabled;
    snapshot_.networkCount = 0;
    scanStartPending_ = false;
    scanActive_ = false;
    scanPurpose_ = ScanPurpose::None;
    connectionActive_ = false;
    candidateCount_ = 0;
    candidatePosition_ = 0;
    snapshot_.autoCandidateCount = 0;
    clearPendingCredentials();

    if (!enabled) {
        esp_wifi_scan_stop();
        WiFi.scanDelete();
        WiFi.disconnect(true, false);
        clearLinkDetails();
        snapshot_.ntpSynced = false;
        snapshot_.utcEpoch = 0;
        timeRequested_ = false;
        wasConnected_ = false;
        syncSavedNetworks();
        setState(WifiState::Disabled, millis());
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    importLegacyProfile();
    syncSavedNetworks();
    WiFi.disconnect(false, false);
    clearLinkDetails();
    wasConnected_ = false;
    if (profiles_.empty()) {
        setState(WifiState::Idle, millis());
    } else {
        requestScan(ScanPurpose::AutoConnect, millis());
    }
}

bool WifiService::startScan() {
    if (!snapshot_.enabled) return false;
    requestScan(ScanPurpose::User, millis());
    return true;
}

bool WifiService::connect(const char* ssid, const char* password) {
    if (!snapshot_.enabled || ssid == nullptr || ssid[0] == '\0') return false;

    const WifiProfile* saved = profiles_.find(ssid);
    const bool useSavedPassword = saved != nullptr &&
                                  (password == nullptr || password[0] == '\0');
    const char* selectedPassword = useSavedPassword ? saved->password.data()
                                                     : (password != nullptr ? password : "");
    return beginConnection(ssid, selectedPassword, !useSavedPassword, false, millis());
}

bool WifiService::forgetNetwork(const char* ssid) {
    const bool forgetAll = ssid == nullptr || ssid[0] == '\0';
    const bool removeActive = forgetAll || sameText(snapshot_.ssid.data(), ssid) ||
                              sameText(pendingSsid_.data(), ssid);

    bool changed = false;
    if (forgetAll) {
        changed = !profiles_.empty();
        profiles_.clear();
    } else {
        changed = profiles_.erase(ssid);
    }
    if (!changed) return false;

    const bool stored = profiles_.empty() ? profileStore_.clear() : persistProfiles();
    syncSavedNetworks();

    if (removeActive || profiles_.empty()) {
        connectionActive_ = false;
        clearPendingCredentials();
        WiFi.disconnect(false, false);
        if (profiles_.empty()) clearLegacyStationConfig();
        wasConnected_ = false;
        clearLinkDetails();
        if (profiles_.empty()) {
            setState(WifiState::Idle, millis());
        } else {
            scheduleAutoScan(millis(), 300);
            setState(WifiState::Idle, millis());
        }
    }
    return stored;
}

void WifiService::update(uint32_t nowMs) {
    if (!snapshot_.enabled) return;

    snapshot_.lastStatus = static_cast<int16_t>(WiFi.status());
    if (snapshot_.state == WifiState::Scanning || scanStartPending_ || scanActive_) {
        processScan(nowMs);
        if (WiFi.status() == WL_CONNECTED) {
            if (!wasConnected_) {
                connectedAtMs_ = nowMs;
                requestTimeSync();
            }
            wasConnected_ = true;
            refreshLinkDetails(nowMs);
        }
        return;
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected) {
        if (!wasConnected_) {
            connectedAtMs_ = nowMs;
            requestTimeSync();
        }
        wasConnected_ = true;
        if (connectionActive_) completeConnection(nowMs);
        setState(WifiState::Connected, nowMs);
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
        connectionActive_ = false;
        clearPendingCredentials();
        setState(WifiState::Idle, nowMs);
        scheduleAutoScan(nowMs, 500);
    }

    if (connectionActive_ && snapshot_.state == WifiState::Connecting) {
        const wl_status_t status = WiFi.status();
        const bool terminal = status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL;
        if (terminal || nowMs - stateSinceMs_ >= kConnectTimeoutMs) {
            failConnection(nowMs);
            return;
        }
    }

    if (!connectionActive_ && !profiles_.empty() && nextAutoScanAtMs_ != 0 &&
        static_cast<int32_t>(nowMs - nextAutoScanAtMs_) >= 0) {
        nextAutoScanAtMs_ = 0;
        requestScan(ScanPurpose::AutoConnect, nowMs);
    }
}

void WifiService::importLegacyProfile() {
    if (!profiles_.empty()) return;

    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK || config.sta.ssid[0] == '\0') {
        return;
    }

    std::array<char, kWifiSsidCapacity> ssid{};
    std::array<char, kWifiPasswordCapacity> password{};
    std::memcpy(ssid.data(), config.sta.ssid,
                std::min<std::size_t>(sizeof(config.sta.ssid), ssid.size() - 1));
    std::memcpy(password.data(), config.sta.password,
                std::min<std::size_t>(sizeof(config.sta.password), password.size() - 1));
    if (profiles_.upsert(ssid.data(), password.data()) && persistProfiles()) {
        // The migrated credential now lives in Pocket Deck's multi-profile
        // record. Remove the legacy SDK station record so it cannot reappear
        // after the user deletes the profile later.
        WiFi.disconnect(false, false);
        clearLegacyStationConfig();
    }
}

bool WifiService::persistProfiles() {
    return profileStore_.save(profiles_);
}

void WifiService::syncSavedNetworks() {
    snapshot_.savedNetworkCount = static_cast<uint8_t>(profiles_.size());
    snapshot_.hasSavedNetwork = !profiles_.empty();
    for (auto& saved : snapshot_.savedNetworks) saved.ssid.fill('\0');
    for (std::size_t index = 0; index < profiles_.size(); ++index) {
        copyText(snapshot_.savedNetworks[index].ssid, profiles_.at(index).ssid.data());
    }
    for (uint8_t index = 0; index < snapshot_.networkCount; ++index) {
        snapshot_.networks[index].saved =
            profiles_.find(snapshot_.networks[index].ssid.data()) != nullptr;
    }
}

void WifiService::requestScan(ScanPurpose purpose, uint32_t nowMs) {
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    scanPurpose_ = purpose;
    scanStartPending_ = true;
    scanActive_ = false;
    scanRequestedAtMs_ = nowMs;
    scanRetryAtMs_ = nowMs;
    candidateCount_ = 0;
    candidatePosition_ = 0;
    snapshot_.autoCandidateCount = 0;
    connectionActive_ = false;
    clearPendingCredentials();
    snapshot_.networkCount = 0;
    snapshot_.lastScanResult = WIFI_SCAN_RUNNING;

    // A pending association attempt makes esp_wifi_scan_start return
    // ESP_ERR_WIFI_STATE. Stop that attempt first; connected stations are
    // allowed to scan without dropping their working link.
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        scanRetryAtMs_ = nowMs + 100;
    }
    setState(WifiState::Scanning, nowMs);
}

void WifiService::processScan(uint32_t nowMs) {
    if (scanStartPending_ && static_cast<int32_t>(nowMs - scanRetryAtMs_) >= 0) {
        const int16_t result = WiFi.scanNetworks(true, true);
        if (result == WIFI_SCAN_FAILED) {
            if (nowMs - scanRequestedAtMs_ >= kScanStartTimeoutMs) {
                failScan(nowMs);
                return;
            }
            esp_wifi_disconnect();
            WiFi.scanDelete();
            scanRetryAtMs_ = nowMs + kScanRetryMs;
        } else if (result == WIFI_SCAN_RUNNING) {
            scanStartPending_ = false;
            scanActive_ = true;
        } else {
            scanStartPending_ = false;
            completeScan(result, nowMs);
            return;
        }
    }

    if (!scanActive_) return;
    const int16_t result = WiFi.scanComplete();
    if (result >= 0) {
        completeScan(result, nowMs);
    } else if (result == WIFI_SCAN_FAILED ||
               nowMs - scanRequestedAtMs_ >= kScanCompleteTimeoutMs) {
        failScan(nowMs);
    }
}

void WifiService::completeScan(int16_t count, uint32_t nowMs) {
    collectScanResults(count);
    const ScanPurpose completedPurpose = scanPurpose_;
    if (completedPurpose == ScanPurpose::AutoConnect) buildCandidates(count);
    WiFi.scanDelete();
    scanStartPending_ = false;
    scanActive_ = false;
    snapshot_.lastScanResult = count;
    ++snapshot_.scanGeneration;
    scanPurpose_ = ScanPurpose::None;

    if (completedPurpose == ScanPurpose::AutoConnect) {
        if (beginNextCandidate(nowMs)) return;
        setState(WifiState::Idle, nowMs);
        scheduleAutoScan(nowMs, kDisconnectedScanIntervalMs);
        return;
    }

    setState(WiFi.status() == WL_CONNECTED ? WifiState::Connected : WifiState::Idle,
             nowMs);
}

void WifiService::failScan(uint32_t nowMs) {
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    scanStartPending_ = false;
    scanActive_ = false;
    snapshot_.lastScanResult = WIFI_SCAN_FAILED;
    ++snapshot_.scanGeneration;
    const ScanPurpose failedPurpose = scanPurpose_;
    scanPurpose_ = ScanPurpose::None;
    setState(WifiState::Error, nowMs);
    if (failedPurpose == ScanPurpose::AutoConnect && !profiles_.empty()) {
        scheduleAutoScan(nowMs, kDisconnectedScanIntervalMs);
    }
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
            if (snapshot_.networkCount < WifiSnapshot::kNetworkCapacity) {
                existing = snapshot_.networkCount++;
            } else {
                uint8_t weakest = 0;
                for (uint8_t saved = 1; saved < snapshot_.networkCount; ++saved) {
                    if (snapshot_.networks[saved].rssi < snapshot_.networks[weakest].rssi) {
                        weakest = saved;
                    }
                }
                if (rssi <= snapshot_.networks[weakest].rssi) continue;
                existing = weakest;
            }
        }

        WifiNetwork& network = snapshot_.networks[existing];
        copyText(network.ssid, ssid.c_str());
        network.rssi = rssi;
        network.channel = static_cast<uint8_t>(WiFi.channel(index));
        network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        network.saved = profiles_.find(ssid.c_str()) != nullptr;
    }

    std::sort(snapshot_.networks.begin(),
              snapshot_.networks.begin() + snapshot_.networkCount,
              [](const WifiNetwork& left, const WifiNetwork& right) {
                  return left.rssi > right.rssi;
              });
}

void WifiService::buildCandidates(int16_t count) {
    candidateCount_ = 0;
    candidatePosition_ = 0;
    std::array<int32_t, kWifiProfileCapacity> strengths{};
    strengths.fill(-127);

    // The UI intentionally shows only the strongest eight networks, but an
    // existing profile may be weaker than that in a crowded location. Build
    // automatic candidates from every raw scan record before deleting it.
    for (int16_t network = 0; network < count; ++network) {
        const String ssid = WiFi.SSID(network);
        if (ssid.isEmpty()) continue;
        const int profile = profiles_.findIndex(ssid.c_str());
        if (profile < 0) continue;

        int candidate = -1;
        for (uint8_t index = 0; index < candidateCount_; ++index) {
            if (candidates_[index] == static_cast<uint8_t>(profile)) {
                candidate = index;
                break;
            }
        }
        const int32_t rssi = WiFi.RSSI(network);
        if (candidate >= 0) {
            strengths[static_cast<std::size_t>(candidate)] =
                std::max(strengths[static_cast<std::size_t>(candidate)], rssi);
        } else if (candidateCount_ < candidates_.size()) {
            candidates_[candidateCount_] = static_cast<uint8_t>(profile);
            strengths[candidateCount_] = rssi;
            ++candidateCount_;
        }
    }

    for (uint8_t left = 0; left < candidateCount_; ++left) {
        for (uint8_t right = left + 1; right < candidateCount_; ++right) {
            if (strengths[right] <= strengths[left]) continue;
            std::swap(strengths[left], strengths[right]);
            std::swap(candidates_[left], candidates_[right]);
        }
    }
    snapshot_.autoCandidateCount = candidateCount_;
}

bool WifiService::beginNextCandidate(uint32_t nowMs) {
    while (candidatePosition_ < candidateCount_) {
        const uint8_t index = candidates_[candidatePosition_++];
        if (index >= profiles_.size()) continue;
        const WifiProfile& profile = profiles_.at(index);
        if (beginConnection(profile.ssid.data(), profile.password.data(), false, true,
                            nowMs)) {
            return true;
        }
    }
    return false;
}

bool WifiService::beginConnection(const char* ssid, const char* password,
                                  bool saveOnSuccess, bool candidateConnection,
                                  uint32_t nowMs) {
    if (ssid == nullptr || ssid[0] == '\0' || password == nullptr) return false;

    esp_wifi_scan_stop();
    WiFi.scanDelete();
    scanStartPending_ = false;
    scanActive_ = false;
    scanPurpose_ = ScanPurpose::None;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    delay(20);

    copyText(pendingSsid_, ssid);
    copyText(pendingPassword_, password);
    saveOnConnect_ = saveOnSuccess;
    candidateConnection_ = candidateConnection;
    connectionActive_ = true;
    copyText(snapshot_.ssid, ssid);
    snapshot_.hasSavedNetwork = !profiles_.empty();
    snapshot_.ntpSynced = false;
    snapshot_.utcEpoch = 0;
    timeRequested_ = false;
    const wl_status_t result = WiFi.begin(ssid, password);
    snapshot_.lastStatus = static_cast<int16_t>(result);
    setState(WifiState::Connecting, nowMs);
    return result != WL_CONNECT_FAILED;
}

void WifiService::completeConnection(uint32_t) {
    if (!connectionActive_) return;

    bool shouldPersist = false;
    if (saveOnConnect_) {
        shouldPersist = profiles_.upsert(pendingSsid_.data(), pendingPassword_.data());
    } else {
        const int existing = profiles_.findIndex(pendingSsid_.data());
        if (existing > 0) shouldPersist = profiles_.touch(pendingSsid_.data());
    }
    if (shouldPersist) persistProfiles();
    syncSavedNetworks();
    connectionActive_ = false;
    candidateConnection_ = false;
    candidateCount_ = 0;
    candidatePosition_ = 0;
    nextAutoScanAtMs_ = 0;
    clearPendingCredentials();
}

void WifiService::failConnection(uint32_t nowMs) {
    const bool tryAnother = candidateConnection_;
    connectionActive_ = false;
    saveOnConnect_ = false;
    candidateConnection_ = false;
    clearPendingCredentials();
    WiFi.disconnect(false, false);

    if (tryAnother && beginNextCandidate(nowMs)) return;
    setState(tryAnother ? WifiState::Idle : WifiState::Error, nowMs);
    if (!profiles_.empty()) scheduleAutoScan(nowMs, kDisconnectedScanIntervalMs);
}

void WifiService::clearPendingCredentials() {
    pendingSsid_.fill('\0');
    pendingPassword_.fill('\0');
    saveOnConnect_ = false;
}

void WifiService::scheduleAutoScan(uint32_t nowMs, uint32_t delayMs) {
    nextAutoScanAtMs_ = nowMs + delayMs;
}

void WifiService::refreshLinkDetails(uint32_t nowMs) {
    snapshot_.connected = true;
    snapshot_.hasSavedNetwork = !profiles_.empty();
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
