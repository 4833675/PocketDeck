#pragma once

#include <array>
#include <cstdint>

namespace pd {

enum class SdLogState : uint8_t {
    Unavailable,
    Ready,
    Formatting,
    Error,
};

struct SdLogSnapshot {
    SdLogState state = SdLogState::Unavailable;
    bool mounted = false;
    uint64_t cardBytes = 0;
    uint64_t usedBytes = 0;
    uint32_t linesWritten = 0;
    std::array<char, 40> error{};
};

class SdLogService {
public:
    static constexpr const char* kDirectory = "/PocketDeck";
    static constexpr const char* kLogPath = "/PocketDeck/ble.log";

    bool begin();
    bool remount();
    bool formatCard();
    bool beginSession(const char* firmwareVersion, const char* resetReason);
    bool append(const char* message);
    SdLogSnapshot snapshot() const { return snapshot_; }

    static void diagnosticsSink(void* context, const char* message);

private:
    bool mount(bool formatIfMissing);
    void unmount();
    bool ensureDirectory();
    bool eraseDirectory(const char* path, uint8_t depth);
    bool rotateIfNeeded();
    void refreshUsage();
    void setError(const char* message, SdLogState state = SdLogState::Error);

    SdLogSnapshot snapshot_{};
    bool ready_ = false;
};

}  // namespace pd
