#include "core/ssh_retry_policy.h"

namespace pd {
namespace {

bool deadlineReached(uint32_t nowMs, uint32_t dueAtMs) {
    return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}

}  // namespace

void SshRetryPolicy::noteFailure(uint32_t nowMs) {
    armed_ = true;
    dueAtMs_ = nowMs + kRetryDelayMs;
}

void SshRetryPolicy::cancel() {
    armed_ = false;
    dueAtMs_ = 0;
}

bool SshRetryPolicy::takeDue(uint32_t nowMs) {
    if (!armed_ || !deadlineReached(nowMs, dueAtMs_)) return false;
    cancel();
    return true;
}

uint32_t SshRetryPolicy::secondsRemaining(uint32_t nowMs) const {
    if (!armed_ || deadlineReached(nowMs, dueAtMs_)) return 0;
    const uint32_t remainingMs = dueAtMs_ - nowMs;
    return (remainingMs + 999) / 1000;
}

}  // namespace pd
