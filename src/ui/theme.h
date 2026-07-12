#pragma once

#include <cstdint>

namespace pd::theme {

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xF8u) << 8u) |
                                 ((green & 0xFCu) << 3u) |
                                 (blue >> 3u));
}

inline constexpr uint16_t kBackground = rgb565(14, 21, 28);
inline constexpr uint16_t kPanel = rgb565(23, 34, 45);
inline constexpr uint16_t kPanelRaised = rgb565(28, 47, 54);
inline constexpr uint16_t kPrimary = rgb565(85, 214, 190);
inline constexpr uint16_t kSecondary = rgb565(155, 124, 255);
inline constexpr uint16_t kText = rgb565(223, 248, 242);
inline constexpr uint16_t kMuted = rgb565(143, 165, 178);
inline constexpr uint16_t kBorder = rgb565(60, 76, 89);
inline constexpr uint16_t kWarning = rgb565(255, 190, 103);
inline constexpr uint16_t kError = rgb565(255, 112, 112);

inline constexpr int16_t kStatusHeight = 16;
inline constexpr int16_t kHintHeight = 16;

}  // namespace pd::theme

