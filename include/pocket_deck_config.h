#pragma once

#include <cstdint>

namespace pd::config {

inline constexpr char kProductName[] = "Pocket Deck";
inline constexpr char kFirmwareVersion[] = "0.1.0";
inline constexpr int16_t kScreenWidth = 240;
inline constexpr int16_t kScreenHeight = 135;
inline constexpr uint32_t kSerialBaud = 115200;
inline constexpr uint32_t kG0LongPressMs = 600;
inline constexpr uint8_t kDefaultBrightness = 78;
inline constexpr uint8_t kDefaultVolume = 55;

}  // namespace pd::config

