#pragma once

#include <cstdint>

namespace pd {

enum class SerialCommand : uint8_t {
    None,
    Help,
    LogStatus,
    LogDump,
    LogDumpAll,
    LogClearConfirmed,
    Unknown,
};

SerialCommand parseSerialCommand(const char* input);

}  // namespace pd
