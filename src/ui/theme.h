#pragma once

#include <array>
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

inline constexpr std::array<uint16_t, 10> kUiPalette{
    kBackground, kPanel, kPanelRaised, kPrimary, kSecondary,
    kText,       kMuted, kBorder,      kWarning, kError,
};

inline constexpr std::array<uint16_t, 16> kAnsiPalette{
    kBackground,
    rgb565(205, 76, 76),
    rgb565(65, 171, 93),
    rgb565(196, 164, 72),
    rgb565(73, 130, 201),
    rgb565(166, 94, 196),
    rgb565(72, 170, 180),
    kText,
    rgb565(92, 108, 119),
    kError,
    rgb565(111, 224, 143),
    rgb565(255, 211, 110),
    rgb565(112, 171, 255),
    rgb565(211, 145, 255),
    rgb565(100, 222, 225),
    kText,
};

inline constexpr int16_t kStatusHeight = 16;
inline constexpr int16_t kHintHeight = 16;

}  // namespace pd::theme
