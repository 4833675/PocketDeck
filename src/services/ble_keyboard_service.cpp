#include "services/ble_keyboard_service.h"

#include <algorithm>
#include <array>
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

bool sameAddress(const uint8_t* left, const uint8_t* right) {
    return std::memcmp(left, right, ESP_BD_ADDR_LEN) == 0;
}

}  // namespace

class BleKeyboardService::ServerCallbacks final : public BLEServerCallbacks {
public:
    explicit ServerCallbacks(BleKeyboardService& owner) : owner_(owner) {}

    void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
        owner_.handleConnect(server, param->connect.conn_id, param->connect.remote_bda);
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

    uint32_t onPassKeyRequest() override { return owner_.passkey_.load(); }
    void onPassKeyNotify(uint32_t passkey) override { owner_.passkey_.store(passkey); }
    bool onSecurityRequest() override { return true; }
    bool onConfirmPIN(uint32_t) override { return true; }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
        owner_.handleAuthentication(result.success, result.fail_reason,
                                    static_cast<uint8_t>(result.auth_mode),
                                    result.key_present);
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
    Serial.printf("[ble] HID ready, bonded=%d\n", bonded_.load() ? 1 : 0);
    return true;
}

void BleKeyboardService::setEnabled(bool enabled) {
    enabled_.store(enabled);
    if (!initialized_.load()) return;
    if (!enabled) {
        disconnect();
        BLEDevice::getAdvertising()->stop();
        reportPolicy_.setConnected(false);
        return;
    }
    startAdvertising();
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
    if (enabled_.load()) startAdvertising();
    Serial.println("[ble] host bond cleared");
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

void BleKeyboardService::handleConnect(BLEServer* server, uint16_t connectionId,
                                       const uint8_t* peerAddress) {
    const bool storedBond = hasStoredBond();
    const bool knownPeer = peerIsBonded(peerAddress);
    if (!BleKeyboardPolicy::peerAllowed(storedBond, knownPeer)) {
        error_.store(BleKeyboardError::UnauthorizedPeer);
        server->disconnect(connectionId);
        Serial.println("[ble] rejected unknown host");
        return;
    }

    connectionId_.store(connectionId);
    connected_.store(true);
    encrypted_.store(false);
    error_.store(BleKeyboardError::None);
    Serial.println("[ble] host connected, requesting encryption");

    // A bonded central does not have to initiate security immediately. Request
    // it here so a reconnect restores the encrypted HID link before macOS
    // decides the peripheral is not ready and drops the connection.
    esp_bd_addr_t encryptionPeer{};
    std::memcpy(encryptionPeer, peerAddress, sizeof(encryptionPeer));
    const esp_err_t result =
        esp_ble_set_encryption(encryptionPeer, ESP_BLE_SEC_ENCRYPT_MITM);
    if (result != ESP_OK) {
        Serial.printf("[ble] encryption request returned 0x%x\n",
                      static_cast<unsigned>(result));
    }
}

void BleKeyboardService::handleDisconnect(uint8_t reason) {
    connected_.store(false);
    encrypted_.store(false);
    lastDisconnectReason_.store(reason);
    reportPolicy_.setConnected(false);
    Serial.printf("[ble] host disconnected, reason=0x%02x\n", reason);
    if (enabled_.load()) startAdvertising();
}

void BleKeyboardService::handleAuthentication(bool success, uint8_t reason, uint8_t authMode,
                                              bool keyPresent) {
    if (!success) {
        encrypted_.store(false);
        error_.store(BleKeyboardError::AuthenticationFailed);
        Serial.printf("[ble] authentication failed: reason=0x%02x auth=0x%02x\n", reason,
                      authMode);
        return;
    }

    const bool bondStored = hasStoredBond();
    encrypted_.store(true);
    bonded_.store(bondStored);
    error_.store(BleKeyboardError::None);
    Serial.printf("[ble] encrypted, bond=%d auth=0x%02x key=%d\n", bondStored ? 1 : 0,
                  authMode, keyPresent ? 1 : 0);
}

void BleKeyboardService::startAdvertising() {
    if (!initialized_.load() || !enabled_.load()) return;
    BLEDevice::startAdvertising();
}

void BleKeyboardService::generatePasskey() {
    passkey_.store(100000u + (esp_random() % 900000u));
}

bool BleKeyboardService::hasStoredBond() const {
    return esp_ble_get_bond_device_num() > 0;
}

bool BleKeyboardService::peerIsBonded(const uint8_t* address) const {
    int count = esp_ble_get_bond_device_num();
    if (count <= 0) return false;
    std::array<esp_ble_bond_dev_t, 8> devices{};
    int listed = std::min<int>(count, devices.size());
    if (esp_ble_get_bond_device_list(&listed, devices.data()) != ESP_OK) return false;
    for (int index = 0; index < listed; ++index) {
        if (sameAddress(devices[index].bd_addr, address)) return true;
    }
    return false;
}

}  // namespace pd
