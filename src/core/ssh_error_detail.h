#pragma once

#include <cstddef>

namespace pd {

void sanitizeSshErrorDetail(const char* detail, const char* hostname,
                            const char* username, char* output,
                            std::size_t capacity);

}  // namespace pd
