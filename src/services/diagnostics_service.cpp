#include "services/diagnostics_service.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace pd {

void DiagnosticsService::log(const char* message) {
    const char* safeMessage = message != nullptr ? message : "";
    auto& destination = messages_[next_];
    destination.fill('\0');
    std::strncpy(destination.data(), safeMessage, destination.size() - 1);

#ifdef ARDUINO
    Serial.printf("[diag] %s\n", safeMessage);
#endif
    if (sink_ != nullptr) sink_(sinkContext_, safeMessage);

    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
}

void DiagnosticsService::logf(const char* format, ...) {
    std::array<char, kAsyncMessageCapacity> formatted{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(formatted.data(), formatted.size(), format, arguments);
    va_end(arguments);
    log(formatted.data());
}

bool DiagnosticsService::enqueue(const char* message) {
    const uint8_t write = pendingWrite_.load(std::memory_order_relaxed);
    const uint8_t next = static_cast<uint8_t>((write + 1) % kAsyncCapacity);
    if (next == pendingRead_.load(std::memory_order_acquire)) {
        pendingDropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    auto& destination = pending_[write];
    destination.fill('\0');
    if (message != nullptr) std::strncpy(destination.data(), message, destination.size() - 1);
    pendingWrite_.store(next, std::memory_order_release);
    return true;
}

bool DiagnosticsService::enqueuef(const char* format, ...) {
    std::array<char, kAsyncMessageCapacity> formatted{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(formatted.data(), formatted.size(), format, arguments);
    va_end(arguments);
    return enqueue(formatted.data());
}

void DiagnosticsService::drainPending() {
    while (true) {
        const uint8_t read = pendingRead_.load(std::memory_order_relaxed);
        if (read == pendingWrite_.load(std::memory_order_acquire)) break;
        const auto message = pending_[read];
        pendingRead_.store(static_cast<uint8_t>((read + 1) % kAsyncCapacity),
                           std::memory_order_release);
        log(message.data());
    }

    const uint32_t dropped = pendingDropped_.exchange(0, std::memory_order_relaxed);
    if (dropped > 0) logf("Async diagnostic events dropped: %lu", static_cast<unsigned long>(dropped));
}

void DiagnosticsService::setSink(Sink sink, void* context, bool replayExisting) {
    sink_ = sink;
    sinkContext_ = context;
    if (!replayExisting || sink_ == nullptr) return;
    for (std::size_t offset = count_; offset > 0; --offset) {
        sink_(sinkContext_, newest(offset - 1));
    }
}

const char* DiagnosticsService::newest(std::size_t offset) const {
    if (offset >= count_) return "";
    const std::size_t index = (next_ + kCapacity - 1 - offset) % kCapacity;
    return messages_[index].data();
}

void DiagnosticsService::clear() {
    for (auto& message : messages_) message.fill('\0');
    next_ = 0;
    count_ = 0;
}

}  // namespace pd
