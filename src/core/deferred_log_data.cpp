#include "core/deferred_log_data.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace pd {
namespace {

constexpr std::array<const char*, 8> kRecorderStates = {
    "no-card", "empty", "ready", "recording", "playing", "unsupported", "malformed", "error",
};
constexpr std::array<const char*, 23> kRecorderErrors = {
    "none", "no-card", "directory-create", "directory-open", "file-open",
    "no-filename", "placeholder-write", "storage-full", "mic-start", "mic-wake-timeout",
    "mic-queue", "mic-drain-timeout", "pcm-write", "checkpoint", "finalize",
    "playback-open", "playback-read", "speaker-start", "speaker-wake-timeout",
    "speaker-queue", "unsupported-wav", "malformed-wav", "delete",
};

bool consume(const char*& cursor, const char* expected) {
    const std::size_t length = std::strlen(expected);
    if (std::strncmp(cursor, expected, length) != 0) return false;
    cursor += length;
    return true;
}

bool parseUnsigned(const char*& cursor, uint32_t& value) {
    if (*cursor < '0' || *cursor > '9') return false;
    uint32_t parsed = 0;
    do {
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        if (parsed > (std::numeric_limits<uint32_t>::max() - digit) / 10u) return false;
        parsed = parsed * 10u + digit;
        ++cursor;
    } while (*cursor >= '0' && *cursor <= '9');
    value = parsed;
    return true;
}

template <std::size_t N>
bool parseToken(const char*& cursor, const std::array<const char*, N>& tokens,
                uint8_t& value) {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const std::size_t length = std::strlen(tokens[index]);
        if (std::strncmp(cursor, tokens[index], length) == 0) {
            cursor += length;
            value = static_cast<uint8_t>(index);
            return true;
        }
    }
    return false;
}

bool finished(const char* cursor) {
    return cursor != nullptr && *cursor == '\0';
}

bool format(char* output, std::size_t capacity, const char* format, uint32_t first,
            uint32_t second, uint32_t third, uint32_t fourth) {
    if (output == nullptr || capacity == 0) return false;
    const int written = std::snprintf(output, capacity, format, static_cast<unsigned>(first),
                                      static_cast<unsigned>(second), static_cast<unsigned>(third),
                                      static_cast<unsigned>(fourth));
    return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

}  // namespace

bool parseDeferredRecorderEvent(const char* message, DeferredLogEvent& event) {
    if (message == nullptr) return false;

    const char* cursor = message;
    DeferredLogEvent parsed = event;
    if (consume(cursor, "RECORDER scan: mounted=")) {
        uint32_t mounted = 0;
        uint32_t ok = 0;
        uint32_t entries = 0;
        uint32_t truncated = 0;
        if (!parseUnsigned(cursor, mounted) || !consume(cursor, " ok=") ||
            !parseUnsigned(cursor, ok) || !consume(cursor, " entries=") ||
            !parseUnsigned(cursor, entries) || !consume(cursor, " truncated=") ||
            !parseUnsigned(cursor, truncated) || !finished(cursor) || mounted > 1u || ok > 1u ||
            entries > 255u || truncated > 1u) {
            return false;
        }
        parsed.kind = DeferredLogEventKind::RecorderScan;
        parsed.payload = static_cast<uint64_t>(mounted) | (static_cast<uint64_t>(ok) << 1u) |
                         (static_cast<uint64_t>(entries) << 2u) |
                         (static_cast<uint64_t>(truncated) << 10u);
        event = parsed;
        return true;
    }

    cursor = message;
    if (consume(cursor, "RECORDER state: ")) {
        uint8_t state = 0;
        uint8_t error = 0;
        if (!parseToken(cursor, kRecorderStates, state) || !consume(cursor, " error=") ||
            !parseToken(cursor, kRecorderErrors, error) || !finished(cursor)) {
            return false;
        }
        parsed.kind = DeferredLogEventKind::RecorderState;
        parsed.payload = static_cast<uint64_t>(state) | (static_cast<uint64_t>(error) << 4u);
        event = parsed;
        return true;
    }

    cursor = message;
    if (consume(cursor, "RECORDER exit: bytes=")) {
        uint32_t bytes = 0;
        uint32_t durationMs = 0;
        if (!parseUnsigned(cursor, bytes) || !consume(cursor, " duration_ms=") ||
            !parseUnsigned(cursor, durationMs) || !finished(cursor)) {
            return false;
        }
        parsed.kind = DeferredLogEventKind::RecorderExit;
        parsed.payload = static_cast<uint64_t>(bytes) |
                         (static_cast<uint64_t>(durationMs) << 32u);
        event = parsed;
        return true;
    }
    return false;
}

bool formatDeferredRecorderEvent(const DeferredLogEvent& event, char* output,
                                 std::size_t capacity) {
    switch (event.kind) {
        case DeferredLogEventKind::RecorderScan:
            return format(output, capacity,
                          "RECORDER scan: mounted=%u ok=%u entries=%u truncated=%u",
                          static_cast<uint32_t>(event.payload & 1u),
                          static_cast<uint32_t>((event.payload >> 1u) & 1u),
                          static_cast<uint32_t>((event.payload >> 2u) & 0xffu),
                          static_cast<uint32_t>((event.payload >> 10u) & 1u));
        case DeferredLogEventKind::RecorderState: {
            const uint8_t state = static_cast<uint8_t>(event.payload & 0x0fu);
            const uint8_t error = static_cast<uint8_t>((event.payload >> 4u) & 0x1fu);
            if (state >= kRecorderStates.size() || error >= kRecorderErrors.size() ||
                output == nullptr || capacity == 0) {
                return false;
            }
            const int written = std::snprintf(output, capacity, "RECORDER state: %s error=%s",
                                              kRecorderStates[state], kRecorderErrors[error]);
            return written >= 0 && static_cast<std::size_t>(written) < capacity;
        }
        case DeferredLogEventKind::RecorderExit:
            return format(output, capacity, "RECORDER exit: bytes=%u duration_ms=%u",
                          static_cast<uint32_t>(event.payload & 0xffffffffu),
                          static_cast<uint32_t>(event.payload >> 32u), 0u, 0u);
        default: return false;
    }
}

bool DeferredLogQueue::push(const DeferredLogEvent& event) {
    if (event.kind == DeferredLogEventKind::None || count_ >= events_.size()) return false;
    const std::size_t index = (head_ + count_) % events_.size();
    events_[index] = event;
    ++count_;
    return true;
}

bool DeferredLogQueue::peek(DeferredLogEvent& event) const {
    if (count_ == 0) return false;
    event = events_[head_];
    return true;
}

bool DeferredLogQueue::pop(DeferredLogEvent& event) {
    if (count_ == 0) return false;
    event = events_[head_];
    events_[head_] = DeferredLogEvent{};
    head_ = (head_ + 1) % events_.size();
    --count_;
    return true;
}

void DeferredLogQueue::clear() {
    for (auto& event : events_) event = DeferredLogEvent{};
    head_ = 0;
    count_ = 0;
}

}  // namespace pd
