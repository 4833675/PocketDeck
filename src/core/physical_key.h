#pragma once

#include <cstdint>

namespace pd {

enum class PhysicalKey : uint8_t {
    None = 0,
    Backtick,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Num0,
    Minus,
    Equal,
    Backspace,
    Tab,
    Q,
    W,
    E,
    R,
    T,
    Y,
    U,
    I,
    O,
    P,
    LeftBracket,
    RightBracket,
    Backslash,
    Fn,
    Shift,
    A,
    S,
    D,
    F,
    G,
    H,
    J,
    K,
    L,
    Semicolon,
    Apostrophe,
    Enter,
    Ctrl,
    Opt,
    Alt,
    Z,
    X,
    C,
    V,
    B,
    N,
    M,
    Comma,
    Period,
    Slash,
    Space,
};

inline constexpr uint8_t kMatrixRows = 4;
inline constexpr uint8_t kMatrixColumns = 14;

constexpr PhysicalKey physicalKeyFromMatrix(uint8_t row, uint8_t column) {
    if (row >= kMatrixRows || column >= kMatrixColumns) return PhysicalKey::None;
    return static_cast<PhysicalKey>(1 + row * kMatrixColumns + column);
}

}  // namespace pd
