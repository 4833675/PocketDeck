#pragma once

#include <atomic>
#include <cstdint>

#include "core/ble_keyboard_policy.h"

struct ble_gap_event;

class NimBLECharacteristic;
class NimBLEHIDDevice;
class NimBLEServer;

namespace pd {

class DiagnosticsService;

enum class BleKeyboardState : uint8_t {
    Disabled,
    Advertising,
    Pairing,
    Connected,
    Error,
};

enum class BleKeyboardError : uint8_t {
    None,
    InitializationFailed,
    UnauthorizedPeer,
    AuthenticationFailed,
    BondOperationFailed,
};

struct BleKeyboardSnapshot {
    BleKeyboardState state = BleKeyboardState::Disabled;
    BleKeyboardError error = BleKeyboardError::None;
    bool enabled = false;
    bool connected = false;
    bool encrypted = false;
    bool bonded = false;
    uint32_t passkey = 0;
    uint8_t lastDisconnectReason = 0;
};

class BleKeyboardService {
public:
    BleKeyboardService() = default;
    ~BleKeyboardService() = default;
    BleKeyboardService(const BleKeyboardService&) = delete;
    BleKeyboardService& operator=(const BleKeyboardService&) = delete;

    bool begin(const char* deviceName);
    void setDiagnostics(DiagnosticsService* diagnostics) { diagnostics_ = diagnostics; }
    void update(uint32_t nowMs);
    void setEnabled(bool enabled);
    bool sendReport(const HidReport& report);
    void updateBattery(uint8_t percent);
    void disconnect();
    bool forgetHost();
    BleKeyboardSnapshot snapshot() const;
    const char* errorText() const;

private:
    class ServerCallbacks;

    friend class ServerCallbacks;

    static int handleGapEvent(ble_gap_event* event, void* argument);
    void handleConnect(uint16_t connectionId, const uint8_t* peerAddress,
                       uint8_t peerAddressType, const uint8_t* peerIdentityAddress,
                       uint8_t peerIdentityAddressType);
    void handleDisconnect(int reason);
    void handleAuthentication(bool encrypted, bool authenticated, bool linkBonded,
                              const uint8_t* peerIdentityAddress,
                              uint8_t peerIdentityAddressType);
    bool allowNewPairing(const char* eventName);
    void startAdvertising();
    void requestAdvertising(uint32_t delayMs);
    void logNow(const char* message);
    void logAsync(const char* message);
    void generatePasskey();
    bool hasStoredBond() const;

    static BleKeyboardService* activeInstance_;

    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_ = nullptr;
    NimBLECharacteristic* inputReport_ = nullptr;
    NimBLECharacteristic* outputReport_ = nullptr;
    ServerCallbacks* serverCallbacks_ = nullptr;
    DiagnosticsService* diagnostics_ = nullptr;
    BleKeyboardPolicy reportPolicy_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{true};
    std::atomic<bool> connected_{false};
    std::atomic<bool> encrypted_{false};
    std::atomic<bool> bonded_{false};
    std::atomic<bool> bondedAtConnect_{false};
    std::atomic<bool> pairingRejected_{false};
    std::atomic<bool> advertisingPending_{false};
    std::atomic<uint32_t> advertisingDueMs_{0};
    std::atomic<uint16_t> connectionId_{0};
    std::atomic<uint32_t> passkey_{0};
    std::atomic<int> pendingDisconnectReason_{-1};
    std::atomic<int> pendingSecurityStatus_{0};
    std::atomic<uint8_t> lastDisconnectReason_{0};
    std::atomic<BleKeyboardError> error_{BleKeyboardError::None};
    uint8_t lastBattery_ = 255;
};

}  // namespace pd
