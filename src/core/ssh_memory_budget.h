#pragma once

#include <cstddef>
#include <cstdint>

namespace pd::ssh_memory {

inline constexpr std::size_t kTransmitCapacity = 512;
inline constexpr std::size_t kReceiveCapacity = 1024;
inline constexpr uint32_t kTaskStackBytes = 20480;

}  // namespace pd::ssh_memory
