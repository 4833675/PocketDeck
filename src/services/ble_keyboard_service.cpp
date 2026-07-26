#include "services/ble_keyboard_service.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <esp_gap_ble_api.h>
#include <esp_system.h>

#include "services/diagnostics_service.h"

namespace pd {
namespace {

constexpr uint8_t kKeyboardReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant)
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array)
    0xC0,              // End Collection
};

void formatAddress(char* output, size_t outputSize, const uint8_t* address) {
    if (address == nullptr) {
        std::snprintf(output, outputSize, "--:--:--:--:--:--");
        return;
    }
    std::snprintf(output, outputSize, "%02X:%02X:%02X:%02X:%02X:%02X", address[0],
                  address[1], address[2], address[3], address[4], address[5]);
}

const char* authenticationFailureName(uint8_t reason) {
    switch (reason) {
        case 0x05: return "hci-auth-failure";
        case 0x06: return "hci-key-missing";
        case 0x50: return "smp-auth-failure";
        case 0x51: return "smp-confirm-failure";
        case 0x52: return "smp-not-supported";
        case 0x56: return "smp-repeated-attempts";
        case 0x61: return "smp-encryption-failure";
        case 0x63: return "smp-response-timeout";
        case 0x66: return "smp-connection-timeout";
        default: return "unknown";
    }
}

}  // namespace

class BleKeyboardService::ServerCallbacks final : public BLEServerCallbacks {
public:
    explicit ServerCallbacks(BleKeyboardService& owner) : owner_(owner) {}

    void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
        owner_.handleConnect(param->connect.conn_id, param->connect.remote_bda,
                             static_cast<uint8_t>(param->connect.ble_addr_type));
    }

    void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
        const uint8_t reason = param != nullptr
                                   ? static_cast<uint8_t>(param->disconnect.reason)
                                   : 0xFF;
        owner_.handleDisconnect(reason);
    }

private:
    BleKeyboardService& owner_;
};

class BleKeyboardService::SecurityCallbacks final : public BLESecurityCallbacks {
public:
    explicit SecurityCallbacks(BleKeyboardService& owner) : owner_(owner) {}

    uint32_t onPassKeyRequest() override {
        if (!owner_.allowNewPairing("passkey request")) return 0;
        return owner_.passkey_.load();
    }
    void onPassKeyNotify(uint32_t passkey) override {
        owner_.handlePasskeyNotification(passkey);
    }
    bool onSecurityRequest() override { return true; }
    bool onConfirmPIN(uint32_t) override {
        return owner_.allowNewPairing("numeric comparison");
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
        owner_.handleAuthentication(result.success, result.fail_reason,
                                    static_cast<uint8_t>(result.auth_mode),
                                    result.key_present, result.bd_addr,
                                    static_cast<uint8_t>(result.addr_type));
    }

private:
    BleKeyboardService& owner_;
};

bool BleKeyboardService::begin(const char* deviceName) {
    if (initialized_.load()) return true;

    generatePasskey();
    BLEDevice::init(deviceName);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    bonded_.store(hasStoredBond());
    server_ = BLEDevice::createServer();
    if (server_ == nullptr) {
        error_.store(BleKeyboardError::InitializationFailed);
        return false;
    }

    serverCallbacks_ = new ServerCallbacks(*this);
    securityCallbacks_ = new SecurityCallbacks(*this);
    server_->setCallbacks(serverCallbacks_);
    BLEDevice::setSecurityCallbacks(securityCallbacks_);

    hid_ = new BLEHIDDevice(server_);
    inputReport_ = hid_->inputReport(1);
    outputReport_ = hid_->outputReport(1);
    hid_->manufacturer()->setValue("Pocket Deck");
    hid_->pnp(0x02, 0x303A, 0x4001, 0x0100);
    hid_->hidInfo(0x00, 0x01);
    hid_->reportMap(const_cast<uint8_t*>(kKeyboardReportMap), sizeof(kKeyboardReportMap));
    hid_->startServices();

    security_ = new BLESecurity();
    security_->setStaticPIN(passkey_.load());
    security_->setCapability(ESP_IO_CAP_OUT);
    security_->setKeySize(16);
    security_->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security_->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security_->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);

    auto* advertising = BLEDevice::getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid_->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);

    initialized_.store(true);
    error_.store(BleKeyboardError::None);
    if (enabled_.load()) startAdvertising();
    char message[64];
    std::snprintf(message, sizeof(message), "BLE HID ready, bonded=%d",
                  bonded_.load() ? 1 : 0);
    logNow(message);
    return true;
}

void BleKeyboardService::update(uint32_t nowMs) {
    if (!advertisingPending_.load() || !enabled_.load() || connected_.load()) return;
    const uint32_t dueMs = advertisingDueMs_.load();
    if (!BleKeyboardPolicy::deadlineReached(nowMs, dueMs)) return;
    if (!advertisingPending_.exchange(false)) return;
    startAdvertising();
}

void BleKeyboardService::setEnabled(bool enabled) {
    enabled_.store(enabled);
    if (!initialized_.load()) return;
    if (!enabled) {
        advertisingPending_.store(false);
        disconnect();
        BLEDevice::getAdvertising()->stop();
        reportPolicy_.setConnected(false);
        return;
    }
    requestAdvertising(0);
}

bool BleKeyboardService::sendReport(const HidReport& report) {
    const bool ready = initialized_.load() && enabled_.load() && connected_.load() &&
                       encrypted_.load() && inputReport_ != nullptr;
    reportPolicy_.setConnected(ready);
    if (!ready) return false;

    const ReportDecision decision = reportPolicy_.nextReport(report);
    if (!decision.send) return false;
    const auto bytes = decision.report.bytes();
    inputReport_->setValue(const_cast<uint8_t*>(bytes.data()), bytes.size());
    inputReport_->notify();
    return true;
}

void BleKeyboardService::updateBattery(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (hid_ == nullptr || percent == lastBattery_) return;
    lastBattery_ = percent;
    hid_->setBatteryLevel(percent);
}

void BleKeyboardService::disconnect() {
    if (server_ != nullptr && connected_.load()) server_->disconnect(connectionId_.load());
    connected_.store(false);
    encrypted_.store(false);
    reportPolicy_.setConnected(false);
}

bool BleKeyboardService::forgetHost() {
    disconnect();
    int count = esp_ble_get_bond_device_num();
    if (count < 0) {
        error_.store(BleKeyboardError::BondOperationFailed);
        return false;
    }

    std::array<esp_ble_bond_dev_t, 8> devices{};
    int listed = std::min<int>(count, devices.size());
    if (listed > 0 && esp_ble_get_bond_device_list(&listed, devices.data()) != ESP_OK) {
        error_.store(BleKeyboardError::BondOperationFailed);
        return false;
    }
    for (int index = 0; index < listed; ++index) {
        if (esp_ble_remove_bond_device(devices[index].bd_addr) != ESP_OK) {
            error_.store(BleKeyboardError::BondOperationFailed);
            return false;
        }
    }

    bonded_.store(false);
    error_.store(BleKeyboardError::None);
    generatePasskey();
    if (security_ != nullptr) {
        security_->setStaticPIN(passkey_.load());
        security_->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    }
    requestAdvertising(350);
    logNow("BLE host bond cleared");
    return true;
}

BleKeyboardSnapshot BleKeyboardService::snapshot() const {
    BleKeyboardSnapshot result;
    result.enabled = enabled_.load();
    result.connected = connected_.load();
    result.encrypted = encrypted_.load();
    result.bonded = bonded_.load();
    result.passkey = passkey_.load();
    result.lastDisconnectReason = lastDisconnectReason_.load();
    result.error = error_.load();

    if (!result.enabled) {
        result.state = BleKeyboardState::Disabled;
    } else if (result.error != BleKeyboardError::None) {
        result.state = BleKeyboardState::Error;
    } else if (result.connected && result.encrypted) {
        result.state = BleKeyboardState::Connected;
    } else if (result.connected || !result.bonded) {
        result.state = BleKeyboardState::Pairing;
    } else {
        result.state = BleKeyboardState::Advertising;
    }
    return result;
}

const char* BleKeyboardService::errorText() const {
    switch (error_.load()) {
        case BleKeyboardError::None: return "none";
        case BleKeyboardError::InitializationFailed: return "initialization failed";
        case BleKeyboardError::UnauthorizedPeer: return "unknown host rejected";
        case BleKeyboardError::AuthenticationFailed: return "authentication failed";
        case BleKeyboardError::BondOperationFailed: return "bond operation failed";
    }
    return "unknown";
}

void BleKeyboardService::handleConnect(uint16_t connectionId, const uint8_t* peerAddress,
                                       uint8_t peerAddressType) {
    advertisingPending_.store(false);
    const int bondCount = esp_ble_get_bond_device_num();
    bondedAtConnect_.store(bondCount > 0);
    pairingRejected_.store(false);
    connectionId_.store(connectionId);
    connected_.store(true);
    encrypted_.store(false);
    error_.store(BleKeyboardError::None);
    char addressText[18];
    formatAddress(addressText, sizeof(addressText), peerAddress);

    // Start security immediately. In ESP-IDF 4.4.7, leaving a bonded HID link
    // waiting for the peer to initiate SMP can end in SMP_CONN_TOUT; Bluedroid
    // then removes the saved bonding keys. The official secure GATT server uses
    // this same explicit encryption request from its connection callback.
    esp_bd_addr_t encryptionPeer{};
    std::memcpy(encryptionPeer, peerAddress, sizeof(encryptionPeer));
    const esp_err_t encryptionResult =
        esp_ble_set_encryption(encryptionPeer, ESP_BLE_SEC_ENCRYPT_MITM);

    char message[DiagnosticsService::kAsyncMessageCapacity];
    std::snprintf(message, sizeof(message),
                  "BLE connect addr=%s type=%u bonds=%d encryption-request=0x%x",
                  addressText, peerAddressType, bondCount,
                  static_cast<unsigned>(encryptionResult));
    logAsync(message);
}

void BleKeyboardService::handleDisconnect(uint8_t reason) {
    connected_.store(false);
    encrypted_.store(false);
    lastDisconnectReason_.store(reason);
    reportPolicy_.setConnected(false);
    char message[80];
    std::snprintf(message, sizeof(message), "BLE disconnect reason=0x%02x", reason);
    logAsync(message);
    const BleKeyboardError currentError = error_.load();
    if (currentError == BleKeyboardError::UnauthorizedPeer ||
        currentError == BleKeyboardError::AuthenticationFailed) {
        error_.store(BleKeyboardError::None);
    }
    requestAdvertising(350);
}

void BleKeyboardService::handleAuthentication(bool success, uint8_t reason, uint8_t authMode,
                                              bool keyPresent, const uint8_t* peerAddress,
                                              uint8_t peerAddressType) {
    if (!success) {
        encrypted_.store(false);
        error_.store(BleKeyboardError::AuthenticationFailed);
        const bool bondAfterFailure = hasStoredBond();
        char addressText[18];
        formatAddress(addressText, sizeof(addressText), peerAddress);
        char message[DiagnosticsService::kAsyncMessageCapacity];
        std::snprintf(message, sizeof(message),
                      "BLE auth failed addr=%s type=%u reason=0x%02x(%s) auth=0x%02x "
                      "bond-before=%d bond-after=%d",
                      addressText, peerAddressType, reason,
                      authenticationFailureName(reason), authMode,
                      bondedAtConnect_.load() ? 1 : 0, bondAfterFailure ? 1 : 0);
        logAsync(message);
        if (server_ != nullptr && connected_.load()) {
            server_->disconnect(connectionId_.load());
        }
        return;
    }

    if (pairingRejected_.load()) {
        logAsync("BLE auth ignored after rejected pairing");
        if (server_ != nullptr && connected_.load()) {
            server_->disconnect(connectionId_.load());
        }
        return;
    }

    const bool bondStored = hasStoredBond();
    encrypted_.store(true);
    bonded_.store(bondStored);
    error_.store(BleKeyboardError::None);
    char addressText[18];
    formatAddress(addressText, sizeof(addressText), peerAddress);
    char message[DiagnosticsService::kAsyncMessageCapacity];
    std::snprintf(message, sizeof(message),
                  "BLE encrypted addr=%s type=%u bond=%d auth=0x%02x key=%d", addressText,
                  peerAddressType, bondStored ? 1 : 0, authMode, keyPresent ? 1 : 0);
    logAsync(message);
}

bool BleKeyboardService::allowNewPairing(const char* eventName) {
    if (BleKeyboardPolicy::newPairingAllowed(bondedAtConnect_.load())) return true;
    if (!pairingRejected_.exchange(true)) {
        error_.store(BleKeyboardError::UnauthorizedPeer);
        char message[DiagnosticsService::kAsyncMessageCapacity];
        std::snprintf(message, sizeof(message),
                      "BLE rejected new pairing (%s); forget host first", eventName);
        logAsync(message);
        if (server_ != nullptr && connected_.load()) {
            server_->disconnect(connectionId_.load());
        }
    }
    return false;
}

void BleKeyboardService::handlePasskeyNotification(uint32_t passkey) {
    if (!allowNewPairing("passkey notification")) return;
    passkey_.store(passkey);
}

void BleKeyboardService::startAdvertising() {
    if (!initialized_.load() || !enabled_.load() || connected_.load()) return;
    BLEDevice::startAdvertising();
    logNow("BLE advertising start requested");
}

void BleKeyboardService::requestAdvertising(uint32_t delayMs) {
    if (!initialized_.load() || !enabled_.load()) return;
    advertisingDueMs_.store(millis() + delayMs);
    advertisingPending_.store(true);
}

void BleKeyboardService::logNow(const char* message) {
    if (diagnostics_ != nullptr) {
        diagnostics_->log(message);
    } else {
        Serial.printf("[ble] %s\n", message != nullptr ? message : "");
    }
}

void BleKeyboardService::logAsync(const char* message) {
    if (diagnostics_ != nullptr && diagnostics_->enqueue(message)) return;
    Serial.printf("[ble] %s\n", message != nullptr ? message : "");
}

void BleKeyboardService::generatePasskey() {
    passkey_.store(100000u + (esp_random() % 900000u));
}

bool BleKeyboardService::hasStoredBond() const {
    return esp_ble_get_bond_device_num() > 0;
}

}  // namespace pd
