#pragma once

#include <array>
#include <cstddef>

namespace pd {

class DiagnosticsService {
public:
    static constexpr std::size_t kCapacity = 12;
    static constexpr std::size_t kMessageCapacity = 48;

    void log(const char* message);
    void logf(const char* format, ...);
    std::size_t size() const { return count_; }
    const char* newest(std::size_t offset) const;
    void clear();

private:
    std::array<std::array<char, kMessageCapacity>, kCapacity> messages_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
};

}  // namespace pd
