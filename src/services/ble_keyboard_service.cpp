#include "services/ble_keyboard_service.h"

#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
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

void formatNativeAddress(char* output, size_t outputSize, const uint8_t* address) {
    if (address == nullptr) {
        std::snprintf(output, outputSize, "--:--:--:--:--:--");
        return;
    }

    // NimBLE stores native BLE addresses least-significant byte first.
    std::snprintf(output, outputSize, "%02X:%02X:%02X:%02X:%02X:%02X", address[5],
                  address[4], address[3], address[2], address[1], address[0]);
}

NimBLEAddress nativeAddress(const uint8_t* address, uint8_t addressType) {
    ble_addr_t value{};
    if (address != nullptr) std::memcpy(value.val, address, sizeof(value.val));
    value.type = addressType;
    return NimBLEAddress(value);
}

}  // namespace

BleKeyboardService* BleKeyboardService::activeInstance_ = nullptr;

class BleKeyboardService::ServerCallbacks final : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleKeyboardService& owner) : owner_(owner) {}

    void onConnect(NimBLEServer*, ble_gap_conn_desc* description) override {
        if (description == nullptr) return;
        owner_.handleConnect(description->conn_handle, description->peer_ota_addr.val,
                             description->peer_ota_addr.type,
                             description->peer_id_addr.val,
                             description->peer_id_addr.type);
    }

    void onDisconnect(NimBLEServer*, ble_gap_conn_desc*) override {
        owner_.handleDisconnect(owner_.pendingDisconnectReason_.exchange(-1));
    }

    uint32_t onPassKeyRequest() override {
        if (!owner_.allowNewPairing("passkey request")) return 0;
        return owner_.passkey_.load();
    }

    bool onConfirmPIN(uint32_t) override {
        return owner_.allowNewPairing("numeric comparison");
    }

    void onAuthenticationComplete(ble_gap_conn_desc* description) override {
        if (description == nullptr) return;
        owner_.handleAuthentication(description->sec_state.encrypted,
                                    description->sec_state.authenticated,
                                    description->sec_state.bonded,
                                    description->peer_id_addr.val,
                                    description->peer_id_addr.type);
    }

private:
    BleKeyboardService& owner_;
};

int BleKeyboardService::handleGapEvent(ble_gap_event* event, void*) {
    BleKeyboardService* instance = activeInstance_;
    if (instance == nullptr || event == nullptr) return 0;

    // NimBLE's public server disconnect callback omits the reason, and its
    // authentication callback omits the security status. A GAP listener sees
    // each event immediately before those callbacks, so retain the diagnostic
    // values without changing NimBLE's handling of the event.
    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        instance->pendingDisconnectReason_.store(event->disconnect.reason);
    } else if (event->type == BLE_GAP_EVENT_ENC_CHANGE) {
        instance->pendingSecurityStatus_.store(event->enc_change.status);
    }
    return 0;
}

bool BleKeyboardService::begin(const char* deviceName) {
    if (initialized_.load()) return true;

    generatePasskey();
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(passkey_.load());
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                     BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                     BLE_SM_PAIR_KEY_DIST_ID);

    activeInstance_ = this;
    NimBLEDevice::setCustomGapHandler(&BleKeyboardService::handleGapEvent);
    bonded_.store(hasStoredBond());

    server_ = NimBLEDevice::createServer();
    if (server_ == nullptr) {
        activeInstance_ = nullptr;
        error_.store(BleKeyboardError::InitializationFailed);
        return false;
    }

    serverCallbacks_ = new ServerCallbacks(*this);
    server_->setCallbacks(serverCallbacks_);
    server_->advertiseOnDisconnect(false);

    hid_ = new NimBLEHIDDevice(server_);
    inputReport_ = hid_->inputReport(1);
    outputReport_ = hid_->outputReport(1);
    hid_->manufacturer()->setValue("Pocket Deck");
    hid_->pnp(0x02, 0x303A, 0x4001, 0x0100);
    hid_->hidInfo(0x00, 0x01);
    hid_->reportMap(const_cast<uint8_t*>(kKeyboardReportMap),
                    sizeof(kKeyboardReportMap));
    hid_->startServices();

    auto* advertising = NimBLEDevice::getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid_->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);

    initialized_.store(true);
    error_.store(BleKeyboardError::None);
    if (enabled_.load()) startAdvertising();
    char message[64];
    std::snprintf(message, sizeof(message), "BLE HID ready (NimBLE), bonded=%d",
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
        NimBLEDevice::stopAdvertising();
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
    if (server_ != nullptr && connected_.load()) {
        server_->disconnect(connectionId_.load());
    }
    connected_.store(false);
    encrypted_.store(false);
    reportPolicy_.setConnected(false);
}

bool BleKeyboardService::forgetHost() {
    disconnect();
    NimBLEDevice::deleteAllBonds();
    if (NimBLEDevice::getNumBonds() != 0) {
        error_.store(BleKeyboardError::BondOperationFailed);
        return false;
    }

    bonded_.store(false);
    bondedAtConnect_.store(false);
    error_.store(BleKeyboardError::None);
    generatePasskey();
    NimBLEDevice::setSecurityPasskey(passkey_.load());
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
                                       uint8_t peerAddressType,
                                       const uint8_t* peerIdentityAddress,
                                       uint8_t peerIdentityAddressType) {
    advertisingPending_.store(false);
    pendingDisconnectReason_.store(-1);
    pendingSecurityStatus_.store(0);

    const int bondCount = NimBLEDevice::getNumBonds();
    const bool hasBond = bondCount > 0;
    const bool knownPeer = hasBond &&
                           NimBLEDevice::isBonded(
                               nativeAddress(peerIdentityAddress, peerIdentityAddressType));
    bondedAtConnect_.store(hasBond);
    pairingRejected_.store(false);
    connectionId_.store(connectionId);
    connected_.store(true);
    encrypted_.store(false);
    error_.store(BleKeyboardError::None);

    char peerText[18];
    char identityText[18];
    formatNativeAddress(peerText, sizeof(peerText), peerAddress);
    formatNativeAddress(identityText, sizeof(identityText), peerIdentityAddress);

    // Enforce the single-host policy before an unknown central can start a new
    // pairing procedure. For a known Mac, NimBLE has already resolved an RPA to
    // the stored identity address by the time this callback runs.
    if (hasBond && !knownPeer) {
        pairingRejected_.store(true);
        error_.store(BleKeyboardError::UnauthorizedPeer);
        char message[DiagnosticsService::kAsyncMessageCapacity];
        std::snprintf(message, sizeof(message),
                      "BLE rejected addr=%s/%u id=%s/%u; stored host differs",
                      peerText, peerAddressType, identityText,
                      peerIdentityAddressType);
        logAsync(message);
        if (server_ != nullptr) server_->disconnect(connectionId);
        return;
    }

    // Explicitly begin link security for both first pairing and bonded
    // reconnects. Unlike the old Bluedroid host, NimBLE keeps the stored bond
    // when a marginal connection times out during this procedure.
    const int securityResult = NimBLEDevice::startSecurity(connectionId);
    char message[DiagnosticsService::kAsyncMessageCapacity];
    std::snprintf(message, sizeof(message),
                  "BLE connect addr=%s/%u id=%s/%u bonds=%d known=%d security=%d",
                  peerText, peerAddressType, identityText, peerIdentityAddressType,
                  bondCount, knownPeer ? 1 : 0, securityResult);
    logAsync(message);
}

void BleKeyboardService::handleDisconnect(int reason) {
    connected_.store(false);
    encrypted_.store(false);
    const uint8_t compactReason =
        reason >= 0 ? static_cast<uint8_t>(reason & 0xFF) : 0xFF;
    lastDisconnectReason_.store(compactReason);
    reportPolicy_.setConnected(false);

    char message[DiagnosticsService::kAsyncMessageCapacity];
    std::snprintf(message, sizeof(message), "BLE disconnect reason=0x%x (%s)",
                  reason, reason >= 0 ? NimBLEUtils::returnCodeToString(reason)
                                      : "not reported");
    logAsync(message);
    const BleKeyboardError currentError = error_.load();
    if (currentError == BleKeyboardError::UnauthorizedPeer ||
        currentError == BleKeyboardError::AuthenticationFailed) {
        error_.store(BleKeyboardError::None);
    }
    requestAdvertising(350);
}

void BleKeyboardService::handleAuthentication(bool encrypted, bool authenticated,
                                              bool linkBonded,
                                              const uint8_t* peerIdentityAddress,
                                              uint8_t peerIdentityAddressType) {
    const int securityStatus = pendingSecurityStatus_.exchange(0);
    char identityText[18];
    formatNativeAddress(identityText, sizeof(identityText), peerIdentityAddress);

    if (!encrypted || !authenticated) {
        encrypted_.store(false);
        if (!pairingRejected_.load()) {
            error_.store(BleKeyboardError::AuthenticationFailed);
        }
        const bool bondAfterFailure = hasStoredBond();
        char message[DiagnosticsService::kAsyncMessageCapacity];
        std::snprintf(message, sizeof(message),
                      "BLE security failed id=%s/%u status=0x%x(%s) enc=%d auth=%d "
                      "bond-before=%d bond-after=%d",
                      identityText, peerIdentityAddressType, securityStatus,
                      NimBLEUtils::returnCodeToString(securityStatus), encrypted ? 1 : 0,
                      authenticated ? 1 : 0, bondedAtConnect_.load() ? 1 : 0,
                      bondAfterFailure ? 1 : 0);
        logAsync(message);
        if (server_ != nullptr && connected_.load()) {
            server_->disconnect(connectionId_.load());
        }
        return;
    }

    if (pairingRejected_.load()) {
        logAsync("BLE security ignored after rejected pairing");
        if (server_ != nullptr && connected_.load()) {
            server_->disconnect(connectionId_.load());
        }
        return;
    }

    const bool bondStored = hasStoredBond();
    encrypted_.store(true);
    bonded_.store(bondStored);
    error_.store(BleKeyboardError::None);
    char message[DiagnosticsService::kAsyncMessageCapacity];
    std::snprintf(message, sizeof(message),
                  "BLE encrypted id=%s/%u stored=%d link-bond=%d auth=%d status=0x%x",
                  identityText, peerIdentityAddressType, bondStored ? 1 : 0,
                  linkBonded ? 1 : 0, authenticated ? 1 : 0, securityStatus);
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

void BleKeyboardService::startAdvertising() {
    if (!initialized_.load() || !enabled_.load() || connected_.load()) return;
    const bool started = NimBLEDevice::startAdvertising();
    char message[72];
    std::snprintf(message, sizeof(message), "BLE advertising start requested result=%d",
                  started ? 1 : 0);
    logNow(message);
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
    return NimBLEDevice::getNumBonds() > 0;
}

}  // namespace pd
