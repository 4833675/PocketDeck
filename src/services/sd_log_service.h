#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/deferred_log_data.h"

class Print;

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
    static constexpr const char* kLogPath = "/PocketDeck/system.log";
    static constexpr std::size_t kDeferredEventCapacity = kDeferredLogQueueCapacity;

    bool begin();
    bool remount();
    bool formatCard();
    bool beginSession(const char* firmwareVersion, const char* resetReason);
    void setDeferred(bool deferred);
    bool deferred() const { return deferred_; }
    bool append(const char* message);
    bool dumpLogs(Print& output, bool includePrevious);
    bool clearLogs();
    SdLogSnapshot snapshot() const { return snapshot_; }

    static void diagnosticsSink(void* context, const char* message);

private:
    bool mount(bool formatIfMissing);
    void unmount();
    bool ensureDirectory();
    bool eraseDirectory(const char* path, uint8_t depth);
    bool dumpFile(Print& output, const char* path);
    bool rotateIfNeeded();
    bool appendNow(const char* message, int64_t epochSeconds, uint32_t uptimeMs);
    bool enqueueDeferred(const char* message);
    bool flushDeferred();
    void noteDeferredDrop();
    void resetDeferredBuffer();
    static DeferredLogEventKind classifyDeferredEvent(const char* message);
    static const char* deferredEventMessage(const DeferredLogEvent& event, char* output,
                                            std::size_t capacity);
    void refreshUsage();
    void setError(const char* message, SdLogState state = SdLogState::Error);

    SdLogSnapshot snapshot_{};
    DeferredLogQueue deferredEvents_{};
    uint32_t deferredDropped_ = 0;
    bool ready_ = false;
    bool deferred_ = false;
    bool flushingDeferred_ = false;
};

}  // namespace pd
