#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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
    static constexpr std::size_t kDeferredEventCapacity = 12;

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
    enum class DeferredEventKind : uint8_t {
        None,
        AppState,
        MediaLibrary,
        MediaPlayback,
        Settings,
        Wifi,
        Bluetooth,
        Gps,
        Weather,
        Storage,
        Ssh,
        Lora,
        System,
        Diagnostics,
    };

    struct DeferredEvent {
        int64_t epochSeconds = 0;
        uint32_t uptimeMs = 0;
        DeferredEventKind kind = DeferredEventKind::None;
    };

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
    static DeferredEventKind classifyDeferredEvent(const char* message);
    static const char* deferredEventMessage(DeferredEventKind kind);
    void refreshUsage();
    void setError(const char* message, SdLogState state = SdLogState::Error);

    SdLogSnapshot snapshot_{};
    std::array<DeferredEvent, kDeferredEventCapacity> deferredEvents_{};
    std::size_t deferredHead_ = 0;
    std::size_t deferredCount_ = 0;
    uint32_t deferredDropped_ = 0;
    bool ready_ = false;
    bool deferred_ = false;
    bool flushingDeferred_ = false;
};

}  // namespace pd
