#include "core/mac_keymap.h"

namespace pd {
namespace {

uint8_t normalUsage(PhysicalKey key) {
    switch (key) {
        case PhysicalKey::Backtick: return 0x35;
        case PhysicalKey::Num1: return 0x1E;
        case PhysicalKey::Num2: return 0x1F;
        case PhysicalKey::Num3: return 0x20;
        case PhysicalKey::Num4: return 0x21;
        case PhysicalKey::Num5: return 0x22;
        case PhysicalKey::Num6: return 0x23;
        case PhysicalKey::Num7: return 0x24;
        case PhysicalKey::Num8: return 0x25;
        case PhysicalKey::Num9: return 0x26;
        case PhysicalKey::Num0: return 0x27;
        case PhysicalKey::Minus: return 0x2D;
        case PhysicalKey::Equal: return 0x2E;
        case PhysicalKey::Backspace: return 0x2A;
        case PhysicalKey::Tab: return 0x2B;
        case PhysicalKey::Q: return 0x14;
        case PhysicalKey::W: return 0x1A;
        case PhysicalKey::E: return 0x08;
        case PhysicalKey::R: return 0x15;
        case PhysicalKey::T: return 0x17;
        case PhysicalKey::Y: return 0x1C;
        case PhysicalKey::U: return 0x18;
        case PhysicalKey::I: return 0x0C;
        case PhysicalKey::O: return 0x12;
        case PhysicalKey::P: return 0x13;
        case PhysicalKey::LeftBracket: return 0x2F;
        case PhysicalKey::RightBracket: return 0x30;
        case PhysicalKey::Backslash: return 0x31;
        case PhysicalKey::A: return 0x04;
        case PhysicalKey::S: return 0x16;
        case PhysicalKey::D: return 0x07;
        case PhysicalKey::F: return 0x09;
        case PhysicalKey::G: return 0x0A;
        case PhysicalKey::H: return 0x0B;
        case PhysicalKey::J: return 0x0D;
        case PhysicalKey::K: return 0x0E;
        case PhysicalKey::L: return 0x0F;
        case PhysicalKey::Semicolon: return 0x33;
        case PhysicalKey::Apostrophe: return 0x34;
        case PhysicalKey::Enter: return 0x28;
        case PhysicalKey::Z: return 0x1D;
        case PhysicalKey::X: return 0x1B;
        case PhysicalKey::C: return 0x06;
        case PhysicalKey::V: return 0x19;
        case PhysicalKey::B: return 0x05;
        case PhysicalKey::N: return 0x11;
        case PhysicalKey::M: return 0x10;
        case PhysicalKey::Comma: return 0x36;
        case PhysicalKey::Period: return 0x37;
        case PhysicalKey::Slash: return 0x38;
        case PhysicalKey::Space: return 0x2C;
        default: return 0;
    }
}

uint8_t fnUsage(PhysicalKey key) {
    switch (key) {
        case PhysicalKey::Backtick: return 0x29;
        case PhysicalKey::Num1: return 0x3A;
        case PhysicalKey::Num2: return 0x3B;
        case PhysicalKey::Num3: return 0x3C;
        case PhysicalKey::Num4: return 0x3D;
        case PhysicalKey::Num5: return 0x3E;
        case PhysicalKey::Num6: return 0x3F;
        case PhysicalKey::Num7: return 0x40;
        case PhysicalKey::Num8: return 0x41;
        case PhysicalKey::Num9: return 0x42;
        case PhysicalKey::Num0: return 0x43;
        case PhysicalKey::Minus: return 0x44;
        case PhysicalKey::Equal: return 0x45;
        case PhysicalKey::Backspace: return 0x4C;
        case PhysicalKey::Tab: return 0x39;
        case PhysicalKey::Semicolon: return 0x52;
        case PhysicalKey::Comma: return 0x50;
        case PhysicalKey::Period: return 0x51;
        case PhysicalKey::Slash: return 0x4F;
        default: return 0;
    }
}

}  // namespace

HidReport MacKeymap::buildReport(const KeyState& state) {
    HidReport report;
    const bool fn = state.contains(PhysicalKey::Fn);
    for (uint8_t i = 0; i < state.count; ++i) {
        switch (state.keys[i]) {
            case PhysicalKey::Ctrl:
                report.modifier |= 0x01;
                break;
            case PhysicalKey::Shift:
                report.modifier |= 0x02;
                break;
            case PhysicalKey::Opt:
                report.modifier |= 0x04;
                break;
            case PhysicalKey::Alt:
                report.modifier |= 0x08;
                break;
            case PhysicalKey::Fn:
                break;
            default:
                report.push(fn ? fnUsage(state.keys[i]) : normalUsage(state.keys[i]));
                break;
        }
    }
    return report;
}

}  // namespace pd
