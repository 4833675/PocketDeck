#include "core/terminal_buffer.h"

#include <cstring>

namespace pd {

TerminalBuffer::TerminalBuffer() {
    reset();
}

void TerminalBuffer::reset() {
    for (auto& line : screen_) line.fill(TerminalCell{});
    for (auto& line : history_) line.fill(TerminalCell{});
    historyStart_ = 0;
    historyCount_ = 0;
    scrollOffset_ = 0;
    cursorRow_ = 0;
    cursorColumn_ = 0;
    currentStyle_ = 0x07;
    parserState_ = ParserState::Normal;
    beginCsi();
}

void TerminalBuffer::write(const char* text) {
    if (text == nullptr) return;
    write(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
}

void TerminalBuffer::write(const uint8_t* bytes, std::size_t length) {
    if (bytes == nullptr) return;
    for (std::size_t index = 0; index < length; ++index) {
        const uint8_t byte = bytes[index];
        if (parserState_ == ParserState::Escape) {
            if (byte == '[') {
                parserState_ = ParserState::Csi;
                beginCsi();
            } else if (byte == ']') {
                parserState_ = ParserState::Osc;
            } else if (byte == '(' || byte == ')') {
                parserState_ = ParserState::IgnoreOne;
            } else {
                parserState_ = ParserState::Normal;
            }
            continue;
        }
        if (parserState_ == ParserState::Osc) {
            if (byte == 0x07) {
                parserState_ = ParserState::Normal;
            } else if (byte == 0x1B) {
                parserState_ = ParserState::OscEscape;
            }
            continue;
        }
        if (parserState_ == ParserState::OscEscape) {
            parserState_ = byte == '\\' || byte == 0x07 ? ParserState::Normal
                                                         : ParserState::Osc;
            continue;
        }
        if (parserState_ == ParserState::IgnoreOne) {
            parserState_ = ParserState::Normal;
            continue;
        }
        if (parserState_ == ParserState::Csi) {
            if (byte >= '0' && byte <= '9') {
                csiHasValue_ = true;
                csiValue_ = static_cast<uint16_t>(csiValue_ * 10u + (byte - '0'));
            } else if (byte == ';') {
                if (csiParameterCount_ < csiParameters_.size()) {
                    csiParameters_[csiParameterCount_++] = csiHasValue_ ? csiValue_ : 0;
                }
                csiValue_ = 0;
                csiHasValue_ = false;
            } else if (byte >= 0x40 && byte <= 0x7E) {
                if (csiParameterCount_ < csiParameters_.size() &&
                    (csiHasValue_ || csiParameterCount_ == 0)) {
                    csiParameters_[csiParameterCount_++] = csiHasValue_ ? csiValue_ : 0;
                }
                finishCsi(byte);
                parserState_ = ParserState::Normal;
            }
            continue;
        }
        if (byte == 0x1B) {
            parserState_ = ParserState::Escape;
            continue;
        }
        if (byte == '\r') {
            cursorColumn_ = 0;
        } else if (byte == '\n') {
            lineFeed();
        } else if (byte == '\b') {
            if (cursorColumn_ > 0) --cursorColumn_;
        } else if (byte == '\t') {
            const std::size_t next = (cursorColumn_ + 8u) & ~std::size_t{7u};
            cursorColumn_ = next < kTerminalColumns ? next : kTerminalColumns - 1;
        } else if (byte >= 0x20 && byte <= 0x7E) {
            screen_[cursorRow_][cursorColumn_].character = static_cast<char>(byte);
            screen_[cursorRow_][cursorColumn_].style = currentStyle_;
            if (cursorColumn_ + 1 < kTerminalColumns) {
                ++cursorColumn_;
            } else {
                cursorColumn_ = 0;
                lineFeed();
            }
        }
    }
}

const TerminalCell& TerminalBuffer::cell(std::size_t row, std::size_t column) const {
    static const TerminalCell blank{};
    if (row >= kTerminalRows || column >= kTerminalColumns) return blank;
    const std::size_t absolute = historyCount_ - scrollOffset_ + row;
    if (absolute < historyCount_) return historyLine(absolute)[column];
    const std::size_t screenRow = absolute - historyCount_;
    return screenRow < kTerminalRows ? screen_[screenRow][column] : blank;
}

bool TerminalBuffer::scrollUp(std::size_t lines) {
    const std::size_t before = scrollOffset_;
    scrollOffset_ = (lines > historyCount_ - scrollOffset_)
                        ? historyCount_
                        : scrollOffset_ + lines;
    return scrollOffset_ != before;
}

bool TerminalBuffer::scrollDown(std::size_t lines) {
    const std::size_t before = scrollOffset_;
    scrollOffset_ = lines >= scrollOffset_ ? 0 : scrollOffset_ - lines;
    return scrollOffset_ != before;
}

void TerminalBuffer::lineFeed() {
    if (cursorRow_ + 1 < kTerminalRows) {
        ++cursorRow_;
        return;
    }
    pushHistory(screen_[0]);
    for (std::size_t row = 0; row + 1 < kTerminalRows; ++row) {
        screen_[row] = screen_[row + 1];
    }
    screen_[kTerminalRows - 1].fill(TerminalCell{});
}

void TerminalBuffer::pushHistory(const Line& line) {
    if (historyCount_ < history_.size()) {
        history_[(historyStart_ + historyCount_) % history_.size()] = line;
        ++historyCount_;
    } else {
        history_[historyStart_] = line;
        historyStart_ = (historyStart_ + 1) % history_.size();
    }
    if (scrollOffset_ > 0 && scrollOffset_ < historyCount_) ++scrollOffset_;
}

const TerminalBuffer::Line& TerminalBuffer::historyLine(
    std::size_t chronologicalIndex) const {
    return history_[(historyStart_ + chronologicalIndex) % history_.size()];
}

void TerminalBuffer::beginCsi() {
    csiParameters_.fill(0);
    csiParameterCount_ = 0;
    csiValue_ = 0;
    csiHasValue_ = false;
}

uint16_t TerminalBuffer::csiParameter(std::size_t index, uint16_t defaultValue) const {
    if (index >= csiParameterCount_ || csiParameters_[index] == 0) return defaultValue;
    return csiParameters_[index];
}

void TerminalBuffer::finishCsi(uint8_t command) {
    if (command == 'm') {
        if (csiParameterCount_ == 0) {
            currentStyle_ = 0x07;
            return;
        }
        for (std::size_t index = 0; index < csiParameterCount_; ++index) {
            const uint16_t parameter = csiParameters_[index];
            uint8_t foreground = static_cast<uint8_t>(currentStyle_ & 0x0Fu);
            uint8_t background = static_cast<uint8_t>((currentStyle_ >> 4u) & 0x0Fu);
            if (parameter == 0) {
                foreground = 7;
                background = 0;
            } else if (parameter == 1 && foreground < 8) {
                foreground = static_cast<uint8_t>(foreground + 8);
            } else if (parameter == 22 && foreground >= 8) {
                foreground = static_cast<uint8_t>(foreground - 8);
            } else if (parameter >= 30 && parameter <= 37) {
                foreground = static_cast<uint8_t>(parameter - 30);
            } else if (parameter == 39) {
                foreground = 7;
            } else if (parameter >= 40 && parameter <= 47) {
                background = static_cast<uint8_t>(parameter - 40);
            } else if (parameter == 49) {
                background = 0;
            } else if (parameter >= 90 && parameter <= 97) {
                foreground = static_cast<uint8_t>(parameter - 90 + 8);
            } else if (parameter >= 100 && parameter <= 107) {
                background = static_cast<uint8_t>(parameter - 100 + 8);
            }
            currentStyle_ = static_cast<uint8_t>((background << 4u) | foreground);
        }
        return;
    }
    if (command == 'H' || command == 'f') {
        const std::size_t row = csiParameter(0, 1) - 1;
        const std::size_t column = csiParameter(1, 1) - 1;
        cursorRow_ = row < kTerminalRows ? row : kTerminalRows - 1;
        cursorColumn_ = column < kTerminalColumns ? column : kTerminalColumns - 1;
        return;
    }
    if (command == 'J') {
        const uint16_t mode = csiParameterCount_ == 0 ? 0 : csiParameters_[0];
        if (mode == 2) {
            for (auto& line : screen_) line.fill(TerminalCell{});
        }
        return;
    }
    if (command == 'D') {
        const std::size_t amount = csiParameter(0, 1);
        cursorColumn_ = amount >= cursorColumn_ ? 0 : cursorColumn_ - amount;
        return;
    }
    if (command == 'A') {
        const std::size_t amount = csiParameter(0, 1);
        cursorRow_ = amount >= cursorRow_ ? 0 : cursorRow_ - amount;
        return;
    }
    if (command == 'B') {
        const std::size_t amount = csiParameter(0, 1);
        cursorRow_ = amount >= kTerminalRows - cursorRow_
                         ? kTerminalRows - 1
                         : cursorRow_ + amount;
        return;
    }
    if (command == 'C') {
        const std::size_t amount = csiParameter(0, 1);
        cursorColumn_ = amount >= kTerminalColumns - cursorColumn_
                            ? kTerminalColumns - 1
                            : cursorColumn_ + amount;
        return;
    }
    if (command == 'K') {
        const uint16_t mode = csiParameterCount_ == 0 ? 0 : csiParameters_[0];
        std::size_t first = cursorColumn_;
        std::size_t last = kTerminalColumns - 1;
        if (mode == 1) {
            first = 0;
            last = cursorColumn_;
        } else if (mode == 2) {
            first = 0;
        }
        for (std::size_t column = first; column <= last; ++column) {
            screen_[cursorRow_][column] = TerminalCell{};
        }
    }
}

}  // namespace pd
