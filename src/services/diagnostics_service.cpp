#include "services/diagnostics_service.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace pd {

void DiagnosticsService::log(const char* message) {
    auto& destination = messages_[next_];
    destination.fill('\0');
    if (message != nullptr) std::strncpy(destination.data(), message, destination.size() - 1);

#ifdef ARDUINO
    Serial.printf("[diag] %s\n", destination.data());
#endif

    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
}

void DiagnosticsService::logf(const char* format, ...) {
    std::array<char, kMessageCapacity> formatted{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(formatted.data(), formatted.size(), format, arguments);
    va_end(arguments);
    log(formatted.data());
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
