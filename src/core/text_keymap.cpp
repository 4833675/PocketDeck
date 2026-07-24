#include "core/text_keymap.h"

namespace pd {

char TextKeymap::character(PhysicalKey key, bool shift) {
    switch (key) {
        case PhysicalKey::Backtick: return shift ? '~' : '`';
        case PhysicalKey::Num1: return shift ? '!' : '1';
        case PhysicalKey::Num2: return shift ? '@' : '2';
        case PhysicalKey::Num3: return shift ? '#' : '3';
        case PhysicalKey::Num4: return shift ? '$' : '4';
        case PhysicalKey::Num5: return shift ? '%' : '5';
        case PhysicalKey::Num6: return shift ? '^' : '6';
        case PhysicalKey::Num7: return shift ? '&' : '7';
        case PhysicalKey::Num8: return shift ? '*' : '8';
        case PhysicalKey::Num9: return shift ? '(' : '9';
        case PhysicalKey::Num0: return shift ? ')' : '0';
        case PhysicalKey::Minus: return shift ? '_' : '-';
        case PhysicalKey::Equal: return shift ? '+' : '=';
        case PhysicalKey::Q: return shift ? 'Q' : 'q';
        case PhysicalKey::W: return shift ? 'W' : 'w';
        case PhysicalKey::E: return shift ? 'E' : 'e';
        case PhysicalKey::R: return shift ? 'R' : 'r';
        case PhysicalKey::T: return shift ? 'T' : 't';
        case PhysicalKey::Y: return shift ? 'Y' : 'y';
        case PhysicalKey::U: return shift ? 'U' : 'u';
        case PhysicalKey::I: return shift ? 'I' : 'i';
        case PhysicalKey::O: return shift ? 'O' : 'o';
        case PhysicalKey::P: return shift ? 'P' : 'p';
        case PhysicalKey::LeftBracket: return shift ? '{' : '[';
        case PhysicalKey::RightBracket: return shift ? '}' : ']';
        case PhysicalKey::Backslash: return shift ? '|' : '\\';
        case PhysicalKey::A: return shift ? 'A' : 'a';
        case PhysicalKey::S: return shift ? 'S' : 's';
        case PhysicalKey::D: return shift ? 'D' : 'd';
        case PhysicalKey::F: return shift ? 'F' : 'f';
        case PhysicalKey::G: return shift ? 'G' : 'g';
        case PhysicalKey::H: return shift ? 'H' : 'h';
        case PhysicalKey::J: return shift ? 'J' : 'j';
        case PhysicalKey::K: return shift ? 'K' : 'k';
        case PhysicalKey::L: return shift ? 'L' : 'l';
        case PhysicalKey::Semicolon: return shift ? ':' : ';';
        case PhysicalKey::Apostrophe: return shift ? '"' : '\'';
        case PhysicalKey::Z: return shift ? 'Z' : 'z';
        case PhysicalKey::X: return shift ? 'X' : 'x';
        case PhysicalKey::C: return shift ? 'C' : 'c';
        case PhysicalKey::V: return shift ? 'V' : 'v';
        case PhysicalKey::B: return shift ? 'B' : 'b';
        case PhysicalKey::N: return shift ? 'N' : 'n';
        case PhysicalKey::M: return shift ? 'M' : 'm';
        case PhysicalKey::Comma: return shift ? '<' : ',';
        case PhysicalKey::Period: return shift ? '>' : '.';
        case PhysicalKey::Slash: return shift ? '?' : '/';
        case PhysicalKey::Space: return ' ';
        default: return '\0';
    }
}

}  // namespace pd
