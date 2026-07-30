#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kMediaTrackCapacity = 32;
inline constexpr std::size_t kMediaPathCapacity = 128;

struct MediaTrack {
    std::array<char, kMediaPathCapacity> path{};
    uint32_t bytes = 0;
};

bool mediaPathIsMp3(const char* path);
const char* mediaTrackName(const MediaTrack& track);
uint8_t mediaProgressPercent(uint32_t position, uint32_t size);

class MediaLibrary {
public:
    void clear();
    bool add(const char* path, uint32_t bytes);
    void sort();
    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    bool truncated() const { return truncated_; }
    std::size_t selectedIndex() const { return selected_; }
    const MediaTrack& at(std::size_t index) const;
    const MediaTrack& selected() const { return at(selected_); }
    void moveSelection(int direction);
    void select(std::size_t index);

private:
    std::array<MediaTrack, kMediaTrackCapacity> tracks_{};
    std::size_t size_ = 0;
    std::size_t selected_ = 0;
    bool truncated_ = false;
};

class MediaElapsedClock {
public:
    void start(uint32_t nowMs);
    void pause(uint32_t nowMs);
    void resume(uint32_t nowMs);
    void stop();
    uint32_t elapsed(uint32_t nowMs) const;
    bool running() const { return running_; }

private:
    uint32_t accumulatedMs_ = 0;
    uint32_t activeSinceMs_ = 0;
    bool running_ = false;
};

}  // namespace pd
