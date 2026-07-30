#pragma once

#include <array>
#include <cstdint>

#include "core/media_data.h"

class AudioFileSourceSD;
class AudioGeneratorMP3;

namespace pd {

class MediaSpeakerOutput;

enum class MediaPlaybackState : uint8_t {
    NoCard,
    Empty,
    Ready,
    Playing,
    Paused,
    Error,
};

const char* mediaPlaybackStateLabel(MediaPlaybackState state);

struct MediaSnapshot {
    MediaPlaybackState state = MediaPlaybackState::NoCard;
    uint8_t trackCount = 0;
    uint8_t selectedIndex = 0;
    uint8_t currentIndex = 0;
    bool hasCurrent = false;
    bool truncated = false;
    uint8_t progressPercent = 0;
    uint32_t elapsedMs = 0;
    std::array<char, 40> detail{};
};

class MediaService {
public:
    MediaService() = default;
    ~MediaService();
    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;

    bool scan(bool storageMounted);
    void update(uint32_t nowMs);
    void stop();
    void moveSelection(int direction);
    bool toggleSelected(uint32_t nowMs);
    bool playRelative(int direction, uint32_t nowMs);
    const MediaLibrary& library() const { return library_; }
    MediaSnapshot snapshot(uint32_t nowMs) const;

private:
    bool startTrack(std::size_t index, uint32_t nowMs);
    void pause(uint32_t nowMs);
    void resume(uint32_t nowMs);
    void releasePlayback(bool stopDecoder);
    void setState(MediaPlaybackState state, const char* detail = nullptr);

    MediaLibrary library_{};
    MediaPlaybackState state_ = MediaPlaybackState::NoCard;
    std::array<char, 40> detail_{};
    std::size_t currentIndex_ = 0;
    bool hasCurrent_ = false;
    MediaElapsedClock elapsed_{};
    AudioFileSourceSD* source_ = nullptr;
    AudioGeneratorMP3* decoder_ = nullptr;
    MediaSpeakerOutput* output_ = nullptr;
};

}  // namespace pd
