#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "core/app_id.h"

namespace pd {

class BleKeyboardService;
class DiagnosticsService;
class GpsService;
class ImuService;
class IrService;
class LoRaService;
class MediaService;
class RecorderService;
class SdLogService;
class SshHostStore;
class SshService;
class WifiService;
class WeatherService;
struct SystemSettings;

struct WifiConnectRequest {
    static constexpr std::size_t kSsidCapacity = 33;
    static constexpr std::size_t kPasswordCapacity = 65;

    std::array<char, kSsidCapacity> ssid{};
    std::array<char, kPasswordCapacity> password{};
};

enum class SystemCommand : uint8_t {
    None,
    ToggleWifi,
    StartWifiScan,
    ConnectWifi,
    ForgetWifi,
    ToggleBluetooth,
    DisconnectBluetooth,
    ForgetHost,
    ToggleLanguage,
    MountStorage,
    FormatStorage,
    AdjustVolume,
    Restart,
    FactoryReset,
};

struct SystemContext {
    AppId requestedApp = AppId::None;
    uint8_t batteryPercent = 0;
    uint8_t volumePercent = 0;
    bool bleEnabled = true;
    bool bleActive = false;
    bool bleConnected = false;
    bool wifiEnabled = false;
    bool wifiActive = false;
    bool wifiConnected = false;
    bool clockValid = false;
    uint8_t clockHour = 0;
    uint8_t clockMinute = 0;
    uint8_t activeModifiers = 0;
    uint32_t uptimeMs = 0;
    uint32_t freeHeap = 0;
    uint32_t minimumFreeHeap = 0;
    const char* resetReason = "unknown";
    BleKeyboardService* bleKeyboard = nullptr;
    GpsService* gps = nullptr;
    ImuService* imu = nullptr;
    IrService* ir = nullptr;
    LoRaService* lora = nullptr;
    MediaService* media = nullptr;
    RecorderService* recorder = nullptr;
    WifiService* wifi = nullptr;
    WeatherService* weather = nullptr;
    SdLogService* sdLog = nullptr;
    SshService* ssh = nullptr;
    SshHostStore* sshHostStore = nullptr;
    DiagnosticsService* diagnostics = nullptr;
    const SystemSettings* settings = nullptr;

    void requestApp(AppId app) { requestedApp = app; }
    AppId takeRequestedApp() {
        const AppId requested = requestedApp;
        requestedApp = AppId::None;
        return requested;
    }

    void requestCommand(SystemCommand command) { requestedCommand_ = command; }
    SystemCommand takeRequestedCommand() {
        const SystemCommand requested = requestedCommand_;
        requestedCommand_ = SystemCommand::None;
        return requested;
    }

    void requestWifiConnect(const char* ssid, const char* password) {
        wifiConnectRequest_.ssid.fill('\0');
        wifiConnectRequest_.password.fill('\0');
        if (ssid != nullptr) {
            std::strncpy(wifiConnectRequest_.ssid.data(), ssid,
                         wifiConnectRequest_.ssid.size() - 1);
        }
        if (password != nullptr) {
            std::strncpy(wifiConnectRequest_.password.data(), password,
                         wifiConnectRequest_.password.size() - 1);
        }
        wifiConnectPending_ = true;
        requestedCommand_ = SystemCommand::ConnectWifi;
    }

    bool takeWifiConnectRequest(WifiConnectRequest& request) {
        if (!wifiConnectPending_) return false;
        request = wifiConnectRequest_;
        wifiConnectRequest_.password.fill('\0');
        wifiConnectPending_ = false;
        return true;
    }

    void requestWifiForget(const char* ssid) {
        wifiForgetSsid_.fill('\0');
        if (ssid != nullptr) {
            std::strncpy(wifiForgetSsid_.data(), ssid, wifiForgetSsid_.size() - 1);
        }
        wifiForgetPending_ = true;
        requestedCommand_ = SystemCommand::ForgetWifi;
    }

    bool takeWifiForgetRequest(std::array<char, WifiConnectRequest::kSsidCapacity>& ssid) {
        if (!wifiForgetPending_) return false;
        ssid = wifiForgetSsid_;
        wifiForgetSsid_.fill('\0');
        wifiForgetPending_ = false;
        return true;
    }

    void requestVolumeDelta(int8_t delta) {
        volumeDelta_ = delta;
        volumeDeltaPending_ = true;
        requestedCommand_ = SystemCommand::AdjustVolume;
    }

    bool takeVolumeDelta(int8_t& delta) {
        if (!volumeDeltaPending_) return false;
        delta = volumeDelta_;
        volumeDelta_ = 0;
        volumeDeltaPending_ = false;
        return true;
    }

private:
    SystemCommand requestedCommand_ = SystemCommand::None;
    WifiConnectRequest wifiConnectRequest_{};
    bool wifiConnectPending_ = false;
    std::array<char, WifiConnectRequest::kSsidCapacity> wifiForgetSsid_{};
    bool wifiForgetPending_ = false;
    int8_t volumeDelta_ = 0;
    bool volumeDeltaPending_ = false;
};

}  // namespace pd
