#include "core/ssh_error_detail.h"

#include <cstring>

namespace pd {
namespace {

bool matches(const char* current, const char* secret, std::size_t secretLength) {
    return secretLength > 0 && std::strncmp(current, secret, secretLength) == 0;
}

bool appendLiteral(char* output, std::size_t capacity, std::size_t& length,
                   const char* literal) {
    const std::size_t literalLength = std::strlen(literal);
    if (length + literalLength >= capacity) return false;
    std::memcpy(output + length, literal, literalLength);
    length += literalLength;
    output[length] = '\0';
    return true;
}

}  // namespace

void sanitizeSshErrorDetail(const char* detail, const char* hostname,
                            const char* username, char* output,
                            std::size_t capacity) {
    if (output == nullptr || capacity == 0) return;
    output[0] = '\0';

    const char* current = detail != nullptr ? detail : "unknown";
    const std::size_t hostnameLength = hostname != nullptr ? std::strlen(hostname) : 0;
    const std::size_t usernameLength = username != nullptr ? std::strlen(username) : 0;
    std::size_t length = 0;

    while (*current != '\0' && length + 1 < capacity) {
        const bool hostMatch = matches(current, hostname, hostnameLength);
        const bool userMatch = matches(current, username, usernameLength);
        if (hostMatch || userMatch) {
            const bool useHost = hostMatch && (!userMatch || hostnameLength >= usernameLength);
            const char* replacement = useHost ? "<host>" : "<user>";
            const std::size_t consumed = useHost ? hostnameLength : usernameLength;
            if (!appendLiteral(output, capacity, length, replacement)) break;
            current += consumed;
            continue;
        }

        const unsigned char character = static_cast<unsigned char>(*current++);
        output[length++] = character == '\r' || character == '\n' || character == '\t'
                               ? ' '
                               : (character >= 0x20 && character <= 0x7e
                                      ? static_cast<char>(character)
                                      : '?');
        output[length] = '\0';
    }
}

}  // namespace pd
