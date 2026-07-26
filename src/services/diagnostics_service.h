#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace pd {

class DiagnosticsService {
public:
    static constexpr std::size_t kCapacity = 12;
    static constexpr std::size_t kMessageCapacity = 48;
    static constexpr std::size_t kAsyncCapacity = 16;
    static constexpr std::size_t kAsyncMessageCapacity = 160;
    using Sink = void (*)(void* context, const char* message);

    void log(const char* message);
    void logf(const char* format, ...);
    bool enqueue(const char* message);
    bool enqueuef(const char* format, ...);
    void drainPending();
    void setSink(Sink sink, void* context, bool replayExisting = false);
    std::size_t size() const { return count_; }
    const char* newest(std::size_t offset) const;
    void clear();

private:
    std::array<std::array<char, kMessageCapacity>, kCapacity> messages_{};
    std::array<std::array<char, kAsyncMessageCapacity>, kAsyncCapacity> pending_{};
    std::atomic<uint8_t> pendingRead_{0};
    std::atomic<uint8_t> pendingWrite_{0};
    std::atomic<uint32_t> pendingDropped_{0};
    Sink sink_ = nullptr;
    void* sinkContext_ = nullptr;
    std::size_t next_ = 0;
    std::size_t count_ = 0;
};

}  // namespace pd
