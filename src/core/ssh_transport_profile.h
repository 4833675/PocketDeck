#pragma once

namespace pd::ssh_transport {

// LibSSH-ESP32 otherwise prefers AES-256-GCM.  On the no-PSRAM Cardputer Adv
// that profile can exhaust the largest contiguous heap block during key
// exchange.  AES-128-CTR is supported by both libssh and the configured host
// while requiring a smaller cipher context.
inline constexpr char kCipher[] = "aes128-ctr";

}  // namespace pd::ssh_transport
