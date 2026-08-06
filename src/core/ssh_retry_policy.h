#pragma once

#include <cstdint>

namespace pd {

class SshRetryPolicy {
public:
    static constexpr uint32_t kRetryDelayMs = 5000;

    void noteFailure(uint32_t nowMs);
    void cancel();
    bool takeDue(uint32_t nowMs);
    uint32_t secondsRemaining(uint32_t nowMs) const;

private:
    bool armed_ = false;
    uint32_t dueAtMs_ = 0;
};

}  // namespace pd
