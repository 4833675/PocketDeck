#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr char kRealtimeAudioDeferredLogStatus[] =
    "TF log access is deferred during realtime audio";
inline constexpr std::size_t kDeferredLogQueueCapacity = 12;

enum class DeferredLogEventKind : uint8_t {
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
    RecorderScan,
    RecorderState,
    RecorderExit,
};

struct DeferredLogEvent {
    int64_t epochSeconds = 0;
    uint32_t uptimeMs = 0;
    DeferredLogEventKind kind = DeferredLogEventKind::None;
    uint64_t payload = 0;
};

bool parseDeferredRecorderEvent(const char* message, DeferredLogEvent& event);
bool formatDeferredRecorderEvent(const DeferredLogEvent& event, char* output,
                                 std::size_t capacity);

class DeferredLogQueue {
public:
    bool push(const DeferredLogEvent& event);
    bool peek(DeferredLogEvent& event) const;
    bool pop(DeferredLogEvent& event);
    void clear();
    bool empty() const { return count_ == 0; }
    std::size_t size() const { return count_; }

private:
    std::array<DeferredLogEvent, kDeferredLogQueueCapacity> events_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
};

}  // namespace pd
