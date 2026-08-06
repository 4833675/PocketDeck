#pragma once

#include <cstdint>

namespace pd {

enum class SshState : uint8_t {
    Idle,
    Connecting,
    Authenticating,
    OpeningShell,
    Connected,
    Disconnected,
    Error,
};

enum class SshError : uint8_t {
    None,
    NoPrivateKey,
    NoNetwork,
    ServiceUnavailable,
    QueueFull,
    SessionCreate,
    Configure,
    Connect,
    KeyImport,
    Authentication,
    ChannelCreate,
    ChannelOpen,
    Pty,
    Shell,
    RemoteClosed,
    Write,
};

struct SshSnapshot {
    SshState state = SshState::Idle;
    SshError error = SshError::None;
    bool keyAvailable = false;
    uint32_t generation = 0;
    uint32_t droppedBytes = 0;
};

}  // namespace pd
