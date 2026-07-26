#include "core/serial_command.h"

#include <array>
#include <cstring>

namespace pd {

SerialCommand parseSerialCommand(const char* input) {
    if (input == nullptr) return SerialCommand::None;

    std::array<char, 48> normalized{};
    std::size_t length = 0;
    bool pendingSpace = false;
    for (const char* cursor = input; *cursor != '\0'; ++cursor) {
        const char value = *cursor;
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
            if (length > 0) pendingSpace = true;
            continue;
        }
        if (pendingSpace && length + 1 < normalized.size()) normalized[length++] = ' ';
        pendingSpace = false;
        if (length + 1 >= normalized.size()) return SerialCommand::Unknown;
        normalized[length++] = value >= 'a' && value <= 'z' ? static_cast<char>(value - 32)
                                                             : value;
    }
    normalized[length] = '\0';

    if (length == 0) return SerialCommand::None;
    if (std::strcmp(normalized.data(), "HELP") == 0) return SerialCommand::Help;
    if (std::strcmp(normalized.data(), "LOG STATUS") == 0) return SerialCommand::LogStatus;
    if (std::strcmp(normalized.data(), "LOG DUMP") == 0) return SerialCommand::LogDump;
    if (std::strcmp(normalized.data(), "LOG DUMP ALL") == 0) return SerialCommand::LogDumpAll;
    if (std::strcmp(normalized.data(), "LOG CLEAR YES") == 0) {
        return SerialCommand::LogClearConfirmed;
    }
    return SerialCommand::Unknown;
}

}  // namespace pd
