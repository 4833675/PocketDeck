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
    uint8_t entryCount = 0;
    uint8_t selectedIndex = 0;
    uint8_t currentIndex = 0;
    bool hasCurrent = false;
    bool selectedDirectory = false;
    bool truncated = false;
    uint8_t directoryDepth = 0;
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
    bool rescan(bool storageMounted);
    void update(uint32_t nowMs);
    void stop();
    void moveSelection(int direction);
    bool enterSelectedDirectory();
    bool goParentDirectory();
    bool toggleSelected(uint32_t nowMs);
    bool playRelative(int direction, uint32_t nowMs);
    const MediaLibrary& library() const { return library_; }
    const char* directoryPath() const { return directoryPath_.data(); }
    bool atRootDirectory() const { return directoryDepth_ == 0; }
    MediaSnapshot snapshot(uint32_t nowMs) const;

private:
    bool loadDirectory(const char* path, uint8_t depth);
    bool findRelativeTrack(std::size_t base, int direction, std::size_t& result) const;
    bool startTrack(std::size_t index, uint32_t nowMs);
    void pause(uint32_t nowMs);
    void resume(uint32_t nowMs);
    void releasePlayback(bool stopDecoder);
    void setState(MediaPlaybackState state, const char* detail = nullptr);

    MediaLibrary library_{};
    std::array<char, kMediaPathCapacity> directoryPath_{};
    uint8_t directoryDepth_ = 0;
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
