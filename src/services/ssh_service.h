#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "core/ssh_hosts.h"

namespace pd {

class DiagnosticsService;

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

const char* sshStateLabel(SshState state);
const char* sshErrorLabel(SshError error);

class SshService {
public:
    bool begin(DiagnosticsService* diagnostics);
    bool connect(const SshHost& host);
    bool disconnect();
    bool send(const uint8_t* bytes, std::size_t length);
    std::size_t read(uint8_t* bytes, std::size_t capacity);
    SshSnapshot snapshot() const;

private:
    enum class ControlType : uint8_t { Connect, Disconnect };
    struct ControlCommand {
        ControlType type = ControlType::Disconnect;
        SshHost host{};
    };

    static void taskEntry(void* context);
    void taskLoop();
    bool establish(const SshHost& host, void* privateKey, void*& session, void*& channel);
    void setState(SshState state, SshError error = SshError::None);
    void addDroppedBytes(std::size_t count);

    DiagnosticsService* diagnostics_ = nullptr;
    QueueHandle_t controlQueue_ = nullptr;
    StreamBufferHandle_t transmitStream_ = nullptr;
    StreamBufferHandle_t receiveStream_ = nullptr;
    SemaphoreHandle_t snapshotMutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    SshSnapshot snapshot_{};
    std::array<char, 120> lastConnectDetail_{};
};

}  // namespace pd
