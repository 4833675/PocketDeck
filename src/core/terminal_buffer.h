#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kTerminalColumns = 40;
inline constexpr std::size_t kTerminalRows = 13;
inline constexpr std::size_t kTerminalScrollbackCapacity = 64;

struct TerminalCell {
    char character = ' ';
    uint8_t style = 0x07;

    uint8_t foreground() const { return style & 0x0Fu; }
    uint8_t background() const { return static_cast<uint8_t>((style >> 4u) & 0x0Fu); }
};

class TerminalBuffer {
public:
    TerminalBuffer();

    void reset();
    void write(const char* text);
    void write(const uint8_t* bytes, std::size_t length);

    const TerminalCell& cell(std::size_t row, std::size_t column) const;
    std::size_t cursorRow() const { return cursorRow_; }
    std::size_t cursorColumn() const { return cursorColumn_; }
    std::size_t scrollbackLines() const { return historyCount_; }
    std::size_t scrollOffset() const { return scrollOffset_; }
    bool scrollUp(std::size_t lines = 1);
    bool scrollDown(std::size_t lines = 1);

private:
    using Line = std::array<TerminalCell, kTerminalColumns>;

    enum class ParserState : uint8_t { Normal, Escape, Csi, Osc, OscEscape, IgnoreOne };

    void lineFeed();
    void pushHistory(const Line& line);
    const Line& historyLine(std::size_t chronologicalIndex) const;
    void beginCsi();
    void finishCsi(uint8_t command);
    uint16_t csiParameter(std::size_t index, uint16_t defaultValue) const;

    std::array<Line, kTerminalRows> screen_{};
    std::array<Line, kTerminalScrollbackCapacity> history_{};
    std::size_t historyStart_ = 0;
    std::size_t historyCount_ = 0;
    std::size_t scrollOffset_ = 0;
    std::size_t cursorRow_ = 0;
    std::size_t cursorColumn_ = 0;
    ParserState parserState_ = ParserState::Normal;
    std::array<uint16_t, 6> csiParameters_{};
    std::size_t csiParameterCount_ = 0;
    uint16_t csiValue_ = 0;
    bool csiHasValue_ = false;
    uint8_t currentStyle_ = 0x07;
};

}  // namespace pd
