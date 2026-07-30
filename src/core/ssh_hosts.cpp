#include "core/ssh_hosts.h"

#include <cstring>

namespace pd {
namespace {

bool validString(const char* value, std::size_t capacity) {
    if (value == nullptr || value[0] == '\0') return false;
    const auto* terminator = static_cast<const char*>(std::memchr(value, '\0', capacity));
    if (terminator == nullptr) return false;
    for (const char* current = value; current < terminator; ++current) {
        if (*current != ' ' && *current != '\t' && *current != '\r' && *current != '\n') {
            return true;
        }
    }
    return false;
}

template <std::size_t N>
void copyString(std::array<char, N>& output, const char* input) {
    output.fill('\0');
    std::strncpy(output.data(), input, output.size() - 1);
}

bool sameEndpoint(const SshHost& host, const char* hostname, const char* username,
                  uint16_t port) {
    return host.port == port && std::strcmp(host.hostname.data(), hostname) == 0 &&
           std::strcmp(host.username.data(), username) == 0;
}

}  // namespace

bool SshHosts::upsert(const char* label, const char* hostname, const char* username,
                      uint16_t port) {
    if (!validString(label, kSshLabelCapacity) ||
        !validString(hostname, kSshHostnameCapacity) ||
        !validString(username, kSshUsernameCapacity) || port == 0) {
        return false;
    }

    std::size_t existing = count_;
    for (std::size_t index = 0; index < count_; ++index) {
        if (sameEndpoint(entries_[index], hostname, username, port)) {
            existing = index;
            break;
        }
    }

    if (existing < count_) {
        for (std::size_t index = existing; index + 1 < count_; ++index) {
            entries_[index] = entries_[index + 1];
        }
    } else if (count_ < entries_.size()) {
        ++count_;
    }

    for (std::size_t index = count_ - 1; index > 0; --index) {
        entries_[index] = entries_[index - 1];
    }
    SshHost host;
    copyString(host.label, label);
    copyString(host.hostname, hostname);
    copyString(host.username, username);
    host.port = port;
    entries_[0] = host;
    return true;
}

bool SshHosts::update(std::size_t index, const char* label, const char* hostname,
                      const char* username, uint16_t port) {
    if (index >= count_ || !validString(label, kSshLabelCapacity) ||
        !validString(hostname, kSshHostnameCapacity) ||
        !validString(username, kSshUsernameCapacity) || port == 0) {
        return false;
    }
    for (std::size_t current = 0; current < count_; ++current) {
        if (current != index && sameEndpoint(entries_[current], hostname, username, port)) {
            return false;
        }
    }
    copyString(entries_[index].label, label);
    copyString(entries_[index].hostname, hostname);
    copyString(entries_[index].username, username);
    entries_[index].port = port;
    return true;
}

bool SshHosts::touch(std::size_t index) {
    if (index >= count_) return false;
    const SshHost selected = entries_[index];
    for (std::size_t current = index; current > 0; --current) {
        entries_[current] = entries_[current - 1];
    }
    entries_[0] = selected;
    return true;
}

bool SshHosts::erase(std::size_t index) {
    if (index >= count_) return false;
    for (std::size_t current = index; current + 1 < count_; ++current) {
        entries_[current] = entries_[current + 1];
    }
    --count_;
    entries_[count_] = SshHost{};
    return true;
}

const SshHost* SshHosts::find(const char* hostname, const char* username,
                              uint16_t port) const {
    if (hostname == nullptr || username == nullptr || port == 0) return nullptr;
    for (std::size_t index = 0; index < count_; ++index) {
        if (sameEndpoint(entries_[index], hostname, username, port)) return &entries_[index];
    }
    return nullptr;
}

}  // namespace pd
