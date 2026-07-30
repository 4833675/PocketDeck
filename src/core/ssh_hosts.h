#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kSshHostCapacity = 6;
inline constexpr std::size_t kSshLabelCapacity = 17;
inline constexpr std::size_t kSshHostnameCapacity = 65;
inline constexpr std::size_t kSshUsernameCapacity = 33;

struct SshHost {
    std::array<char, kSshLabelCapacity> label{};
    std::array<char, kSshHostnameCapacity> hostname{};
    std::array<char, kSshUsernameCapacity> username{};
    uint16_t port = 22;
};

class SshHosts {
public:
    bool empty() const { return count_ == 0; }
    std::size_t size() const { return count_; }
    const SshHost& at(std::size_t index) const { return entries_[index]; }

    bool upsert(const char* label, const char* hostname, const char* username,
                uint16_t port);
    bool update(std::size_t index, const char* label, const char* hostname,
                const char* username, uint16_t port);
    bool touch(std::size_t index);
    bool erase(std::size_t index);
    const SshHost* find(const char* hostname, const char* username, uint16_t port) const;

private:
    std::array<SshHost, kSshHostCapacity> entries_{};
    std::size_t count_ = 0;
};

}  // namespace pd
