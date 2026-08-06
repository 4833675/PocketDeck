#include "services/ssh_service.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <libssh/callbacks.h>
#include <libssh/libssh.h>

#include "core/ssh_error_detail.h"
#include "core/ssh_memory_budget.h"
#include "core/ssh_transport_profile.h"
#include "services/diagnostics_service.h"
#include "ssh_private_key.h"

namespace pd {
namespace {

constexpr long kConnectTimeoutSeconds = 8;

const char* connectFailureClass(const char* detail, int socketError) {
    if (socketError == ECONNREFUSED) return "refused";
    if (socketError == ETIMEDOUT) return "timeout";
    if (socketError == ENETUNREACH || socketError == EHOSTUNREACH) return "unreachable";
    if (socketError == ENOMEM) return "memory";
    if (detail == nullptr) return "unknown";
    if (std::strstr(detail, "resolve") != nullptr ||
        std::strstr(detail, "Name or service") != nullptr) {
        return "dns";
    }
    if (std::strstr(detail, "refused") != nullptr) return "refused";
    if (std::strstr(detail, "timed out") != nullptr ||
        std::strstr(detail, "Timeout") != nullptr) {
        return "timeout";
    }
    if (std::strstr(detail, "memory") != nullptr ||
        std::strstr(detail, "alloc") != nullptr) {
        return "memory";
    }
    return "other";
}

void closeConnection(ssh_channel& channel, ssh_session& session) {
    if (channel != nullptr) {
        if (ssh_channel_is_open(channel)) {
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
        }
        ssh_channel_free(channel);
        channel = nullptr;
    }
    if (session != nullptr) {
        if (ssh_is_connected(session)) ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
    }
}

}  // namespace

const char* sshStateLabel(SshState state) {
    switch (state) {
        case SshState::Idle: return "READY";
        case SshState::Connecting: return "CONNECTING";
        case SshState::Authenticating: return "AUTHENTICATING";
        case SshState::OpeningShell: return "OPENING SHELL";
        case SshState::Connected: return "CONNECTED";
        case SshState::Disconnected: return "DISCONNECTED";
        case SshState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* sshErrorLabel(SshError error) {
    switch (error) {
        case SshError::None: return "None";
        case SshError::NoPrivateKey: return "SSH key missing";
        case SshError::NoNetwork: return "Wi-Fi not connected";
        case SshError::ServiceUnavailable: return "SSH service unavailable";
        case SshError::QueueFull: return "SSH command queue full";
        case SshError::SessionCreate: return "Could not create session";
        case SshError::Configure: return "Session configuration failed";
        case SshError::Connect: return "Connection failed";
        case SshError::KeyImport: return "Private key import failed";
        case SshError::Authentication: return "Public-key authentication failed";
        case SshError::ChannelCreate: return "Could not create channel";
        case SshError::ChannelOpen: return "Could not open channel";
        case SshError::Pty: return "PTY request failed";
        case SshError::Shell: return "Shell request failed";
        case SshError::RemoteClosed: return "Remote shell closed";
        case SshError::Write: return "Terminal write failed";
    }
    return "Unknown SSH error";
}

bool SshService::begin(DiagnosticsService* diagnostics) {
    if (task_ != nullptr) return true;
    diagnostics_ = diagnostics;
    if (diagnostics_ != nullptr) {
        diagnostics_->logf("SSH init heap free=%lu largest=%lu",
                           static_cast<unsigned long>(ESP.getFreeHeap()),
                           static_cast<unsigned long>(
                               heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    }
    snapshot_.keyAvailable = generated::kSshPrivateKeyAvailable;
    controlQueue_ = xQueueCreate(4, sizeof(ControlCommand));
    transmitStream_ = xStreamBufferCreate(ssh_memory::kTransmitCapacity, 1);
    receiveStream_ = xStreamBufferCreate(ssh_memory::kReceiveCapacity, 1);
    snapshotMutex_ = xSemaphoreCreateMutex();
    if (diagnostics_ != nullptr) {
        diagnostics_->logf("SSH resources q=%d tx=%d rx=%d lock=%d free=%lu largest=%lu",
                           controlQueue_ != nullptr ? 1 : 0,
                           transmitStream_ != nullptr ? 1 : 0,
                           receiveStream_ != nullptr ? 1 : 0,
                           snapshotMutex_ != nullptr ? 1 : 0,
                           static_cast<unsigned long>(ESP.getFreeHeap()),
                           static_cast<unsigned long>(
                               heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    }
    if (controlQueue_ == nullptr || transmitStream_ == nullptr ||
        receiveStream_ == nullptr || snapshotMutex_ == nullptr) {
        setState(SshState::Error, SshError::ServiceUnavailable);
        return false;
    }
    const BaseType_t created = xTaskCreatePinnedToCore(
        &SshService::taskEntry, "pocket-ssh", ssh_memory::kTaskStackBytes, this,
        tskIDLE_PRIORITY + 1, &task_, portNUM_PROCESSORS - 1);
    if (created != pdPASS) {
        if (diagnostics_ != nullptr) {
            diagnostics_->logf("SSH task create failed rc=%ld free=%lu largest=%lu stack=%lu",
                               static_cast<long>(created),
                               static_cast<unsigned long>(ESP.getFreeHeap()),
                               static_cast<unsigned long>(
                                   heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                               static_cast<unsigned long>(ssh_memory::kTaskStackBytes));
        }
        setState(SshState::Error, SshError::ServiceUnavailable);
        return false;
    }
    setState(SshState::Idle);
    return true;
}

bool SshService::connect(const SshHost& host) {
    if (controlQueue_ == nullptr) return false;
    SshHosts validation;
    if (!validation.upsert(host.label.data(), host.hostname.data(), host.username.data(),
                           host.port)) {
        return false;
    }
    ControlCommand command;
    command.type = ControlType::Connect;
    command.host = host;
    if (xQueueSend(controlQueue_, &command, 0) == pdTRUE) return true;
    setState(SshState::Error, SshError::QueueFull);
    return false;
}

bool SshService::disconnect() {
    if (controlQueue_ == nullptr) return false;
    ControlCommand command;
    command.type = ControlType::Disconnect;
    return xQueueSend(controlQueue_, &command, 0) == pdTRUE;
}

bool SshService::send(const uint8_t* bytes, std::size_t length) {
    if (bytes == nullptr || length == 0 || transmitStream_ == nullptr ||
        snapshot().state != SshState::Connected) {
        return false;
    }
    return xStreamBufferSend(transmitStream_, bytes, length, 0) == length;
}

std::size_t SshService::read(uint8_t* bytes, std::size_t capacity) {
    if (bytes == nullptr || capacity == 0 || receiveStream_ == nullptr) return 0;
    return xStreamBufferReceive(receiveStream_, bytes, capacity, 0);
}

SshSnapshot SshService::snapshot() const {
    if (snapshotMutex_ == nullptr) return snapshot_;
    xSemaphoreTake(snapshotMutex_, portMAX_DELAY);
    const SshSnapshot copy = snapshot_;
    xSemaphoreGive(snapshotMutex_);
    return copy;
}

void SshService::taskEntry(void* context) {
    static_cast<SshService*>(context)->taskLoop();
}

void SshService::connectStatusCallback(void* context, float status) {
    auto* service = static_cast<SshService*>(context);
    if (service == nullptr) return;
    const uint8_t stage = static_cast<uint8_t>(status * 100.0f + 0.5f);
    service->lastConnectStage_ = stage;
}

void SshService::taskLoop() {
    if (ssh_init() != SSH_OK) {
        setState(SshState::Error, SshError::ServiceUnavailable);
        vTaskDelete(nullptr);
        return;
    }
    if (diagnostics_ != nullptr) {
        diagnostics_->enqueuef(
            "SSH task ready free=%lu largest=%lu stack-free=%lu",
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    ssh_key privateKey = nullptr;
    if (generated::kSshPrivateKeyAvailable) {
        const uint32_t beforeFree = ESP.getFreeHeap();
        const uint32_t beforeLargest =
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        const int keyImportResult = ssh_pki_import_privkey_base64(
            reinterpret_cast<const char*>(generated::kSshPrivateKey), nullptr,
            nullptr, nullptr, &privateKey);
        if (diagnostics_ != nullptr) {
            diagnostics_->enqueuef(
                "SSH key cache rc=%d before=%lu/%lu after=%lu/%lu stack-free=%lu",
                keyImportResult, static_cast<unsigned long>(beforeFree),
                static_cast<unsigned long>(beforeLargest),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
        }
    }

    ssh_session session = nullptr;
    ssh_channel channel = nullptr;
    std::array<uint8_t, 128> pendingWrite{};
    std::size_t pendingLength = 0;
    std::size_t pendingOffset = 0;

    while (true) {
        ControlCommand command;
        const TickType_t wait = session == nullptr ? portMAX_DELAY : 0;
        if (xQueueReceive(controlQueue_, &command, wait) == pdTRUE) {
            if (command.type == ControlType::Disconnect) {
                closeConnection(channel, session);
                xStreamBufferReset(transmitStream_);
                xStreamBufferReset(receiveStream_);
                pendingLength = 0;
                pendingOffset = 0;
                setState(SshState::Idle);
                continue;
            }

            closeConnection(channel, session);
            xStreamBufferReset(transmitStream_);
            xStreamBufferReset(receiveStream_);
            pendingLength = 0;
            pendingOffset = 0;
            void* rawSession = nullptr;
            void* rawChannel = nullptr;
            if (!establish(command.host, privateKey, rawSession, rawChannel)) continue;
            session = static_cast<ssh_session>(rawSession);
            channel = static_cast<ssh_channel>(rawChannel);
        }

        if (session == nullptr || channel == nullptr) continue;
        if (WiFi.status() != WL_CONNECTED) {
            closeConnection(channel, session);
            setState(SshState::Disconnected, SshError::NoNetwork);
            continue;
        }
        if (!ssh_is_connected(session) || ssh_channel_is_closed(channel) ||
            ssh_channel_is_eof(channel)) {
            closeConnection(channel, session);
            setState(SshState::Disconnected, SshError::RemoteClosed);
            continue;
        }

        std::array<uint8_t, 256> incoming{};
        for (int stream = 0; stream < 2; ++stream) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                const int received = ssh_channel_read_nonblocking(
                    channel, incoming.data(), incoming.size(), stream);
                if (received <= 0) break;
                const std::size_t accepted = xStreamBufferSend(
                    receiveStream_, incoming.data(), static_cast<std::size_t>(received),
                    pdMS_TO_TICKS(20));
                if (accepted < static_cast<std::size_t>(received)) {
                    addDroppedBytes(static_cast<std::size_t>(received) - accepted);
                }
            }
        }

        if (pendingOffset >= pendingLength) {
            pendingLength = xStreamBufferReceive(transmitStream_, pendingWrite.data(),
                                                  pendingWrite.size(), 0);
            pendingOffset = 0;
        }
        if (pendingOffset < pendingLength) {
            const int written = ssh_channel_write(
                channel, pendingWrite.data() + pendingOffset,
                static_cast<uint32_t>(pendingLength - pendingOffset));
            if (written > 0) {
                pendingOffset += static_cast<std::size_t>(written);
            } else if (written == SSH_ERROR) {
                closeConnection(channel, session);
                setState(SshState::Disconnected, SshError::Write);
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

bool SshService::establish(const SshHost& host, void* rawPrivateKey,
                           void*& rawSession, void*& rawChannel) {
    rawSession = nullptr;
    rawChannel = nullptr;
    setState(SshState::Connecting);
    if (!generated::kSshPrivateKeyAvailable) {
        setState(SshState::Error, SshError::NoPrivateKey);
        return false;
    }
    if (rawPrivateKey == nullptr) {
        setState(SshState::Error, SshError::KeyImport);
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        setState(SshState::Error, SshError::NoNetwork);
        return false;
    }

    ssh_session session = ssh_new();
    ssh_channel channel = nullptr;
    if (session == nullptr) {
        setState(SshState::Error, SshError::SessionCreate);
        return false;
    }
    static ssh_callbacks_struct callbacks{};
    callbacks = {};
    callbacks.userdata = this;
    callbacks.connect_status_function = &SshService::connectStatusCallback;
    ssh_callbacks_init(&callbacks);
    lastConnectStage_ = 0;
    if (ssh_set_callbacks(session, &callbacks) != SSH_OK) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Configure);
        return false;
    }
    const unsigned int port = host.port;
    const int verbosity = SSH_LOG_NOLOG;
    if (ssh_options_set(session, SSH_OPTIONS_HOST, host.hostname.data()) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_USER, host.username.data()) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_PORT, &port) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &kConnectTimeoutSeconds) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_CIPHERS_C_S,
                        ssh_transport::kCipher) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_CIPHERS_S_C,
                        ssh_transport::kCipher) != SSH_OK ||
        ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity) != SSH_OK) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Configure);
        return false;
    }
    if (diagnostics_ != nullptr) {
        diagnostics_->enqueuef(
            "SSH transport profile cipher=%s free=%lu largest=%lu stack-free=%lu",
            ssh_transport::kCipher, static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    if (ssh_connect(session) != SSH_OK) {
        const int socketError = errno;
        const int libsshError = ssh_get_error_code(session);
        std::array<char, 120> safeDetail{};
        sanitizeSshErrorDetail(ssh_get_error(session), host.hostname.data(),
                               host.username.data(), safeDetail.data(), safeDetail.size());
        if (diagnostics_ != nullptr) {
            if (safeDetail != lastConnectDetail_) {
                diagnostics_->enqueuef("SSH connect detail: %s", safeDetail.data());
                lastConnectDetail_ = safeDetail;
            }
            diagnostics_->enqueuef(
                "SSH connect fail stage=%u class=%s code=%d errno=%d free=%lu largest=%lu stack-free=%lu",
                static_cast<unsigned>(lastConnectStage_),
                connectFailureClass(ssh_get_error(session), socketError), libsshError,
                socketError, static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
        }
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Connect);
        return false;
    }

    if (diagnostics_ != nullptr) {
        diagnostics_->enqueuef(
            "SSH transport ready free=%lu largest=%lu stack-free=%lu",
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    }

    setState(SshState::Authenticating);
    const uint32_t authenticationStartedMs = millis();
    const int authentication = ssh_userauth_publickey(
        session, nullptr, static_cast<ssh_key>(rawPrivateKey));
    if (authentication != SSH_AUTH_SUCCESS) {
        std::array<char, 120> safeDetail{};
        sanitizeSshErrorDetail(ssh_get_error(session), host.hostname.data(),
                               host.username.data(), safeDetail.data(), safeDetail.size());
        if (diagnostics_ != nullptr) {
            diagnostics_->enqueuef("SSH auth detail: %s", safeDetail.data());
            diagnostics_->enqueuef(
                "SSH auth fail rc=%d ms=%lu free=%lu largest=%lu stack-free=%lu",
                authentication,
                static_cast<unsigned long>(millis() - authenticationStartedMs),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
        }
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Authentication);
        return false;
    }

    setState(SshState::OpeningShell);
    channel = ssh_channel_new(session);
    if (channel == nullptr) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::ChannelCreate);
        return false;
    }
    if (ssh_channel_open_session(channel) != SSH_OK) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::ChannelOpen);
        return false;
    }
    if (ssh_channel_request_pty_size(channel, "xterm-256color", 40, 13) != SSH_OK) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Pty);
        return false;
    }
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        closeConnection(channel, session);
        setState(SshState::Error, SshError::Shell);
        return false;
    }
    ssh_set_blocking(session, 0);
    rawSession = session;
    rawChannel = channel;
    setState(SshState::Connected);
    if (diagnostics_ != nullptr) {
        diagnostics_->enqueuef(
            "SSH shell ready free=%lu largest=%lu stack-free=%lu",
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    return true;
}

void SshService::setState(SshState state, SshError error) {
    bool changed = false;
    if (snapshotMutex_ != nullptr) xSemaphoreTake(snapshotMutex_, portMAX_DELAY);
    if (snapshot_.state != state || snapshot_.error != error) {
        snapshot_.state = state;
        snapshot_.error = error;
        ++snapshot_.generation;
        changed = true;
    }
    snapshot_.keyAvailable = generated::kSshPrivateKeyAvailable;
    if (snapshotMutex_ != nullptr) xSemaphoreGive(snapshotMutex_);
    if (changed && diagnostics_ != nullptr) {
        diagnostics_->enqueuef("SSH state: %s (%s)", sshStateLabel(state),
                               sshErrorLabel(error));
    }
}

void SshService::addDroppedBytes(std::size_t count) {
    if (snapshotMutex_ != nullptr) xSemaphoreTake(snapshotMutex_, portMAX_DELAY);
    snapshot_.droppedBytes += static_cast<uint32_t>(count);
    if (snapshotMutex_ != nullptr) xSemaphoreGive(snapshotMutex_);
}

}  // namespace pd
