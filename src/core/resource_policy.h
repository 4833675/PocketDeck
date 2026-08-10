#pragma once

#include <cstdint>

#include "core/app_id.h"

namespace pd {

// Services are initialized by System, while this portable policy decides which
// workloads are allowed to run for the foreground app. User settings remain
// the master permission for Wi-Fi and BLE; a profile can request a radio but
// cannot override a disabled setting.
enum class RuntimeResource : uint8_t {
    Ble = 1u << 0u,
    Wifi = 1u << 1u,
    Gps = 1u << 2u,
    LoRa = 1u << 3u,
    MediaRealtime = 1u << 4u,
    Imu = 1u << 5u,
    Ir = 1u << 6u,
    RecorderRealtime = 1u << 7u,
};

struct AppResourceProfile {
    uint8_t mask = 0;

    constexpr bool needs(RuntimeResource resource) const {
        return (mask & static_cast<uint8_t>(resource)) != 0;
    }
};

constexpr uint8_t resourceMask(RuntimeResource resource) {
    return static_cast<uint8_t>(resource);
}

template <typename... Resources>
constexpr AppResourceProfile makeResourceProfile(Resources... resources) {
    return {static_cast<uint8_t>((0u | ... | resourceMask(resources)))};
}

constexpr AppResourceProfile resourceProfileFor(AppId app) {
    switch (app) {
        case AppId::Keyboard:
            return makeResourceProfile(RuntimeResource::Ble);
        case AppId::Ssh:
            return makeResourceProfile(RuntimeResource::Wifi);
        case AppId::Gps:
            return makeResourceProfile(RuntimeResource::Gps);
        case AppId::Motion:
            return makeResourceProfile(RuntimeResource::Imu);
        case AppId::Remote:
            return makeResourceProfile(RuntimeResource::Ir);
        case AppId::LoRa:
            return makeResourceProfile(RuntimeResource::LoRa);
        case AppId::Media:
            return makeResourceProfile(RuntimeResource::MediaRealtime);
        case AppId::Recorder:
            return makeResourceProfile(RuntimeResource::RecorderRealtime);
        case AppId::Weather:
            return makeResourceProfile(RuntimeResource::Wifi, RuntimeResource::Gps);
        case AppId::Settings:
            // Settings is the only place outside the feature apps where the
            // user can scan Wi-Fi or manage the single BLE host.
            return makeResourceProfile(RuntimeResource::Wifi, RuntimeResource::Ble);
        case AppId::Launcher:
        case AppId::None:
            return {};
    }
    return {};
}

}  // namespace pd
