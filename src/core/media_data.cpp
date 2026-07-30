#include "core/media_data.h"

#include <cstring>

namespace pd {
namespace {

char asciiLower(char value) {
    if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
    return value;
}

int compareAsciiCaseInsensitive(const char* lhs, const char* rhs) {
    while (*lhs != '\0' && *rhs != '\0') {
        const char left = asciiLower(*lhs);
        const char right = asciiLower(*rhs);
        if (left != right) return left < right ? -1 : 1;
        ++lhs;
        ++rhs;
    }
    if (*lhs == *rhs) return 0;
    return *lhs == '\0' ? -1 : 1;
}

int compareTracks(const MediaTrack& lhs, const MediaTrack& rhs) {
    const int byName = compareAsciiCaseInsensitive(mediaTrackName(lhs), mediaTrackName(rhs));
    if (byName != 0) return byName;
    return std::strcmp(lhs.path.data(), rhs.path.data());
}

}  // namespace

bool mediaPathIsMp3(const char* path) {
    if (path == nullptr) return false;
    const std::size_t length = std::strlen(path);
    if (length <= 4) return false;
    return path[length - 4] == '.' && asciiLower(path[length - 3]) == 'm' &&
           asciiLower(path[length - 2]) == 'p' && asciiLower(path[length - 1]) == '3';
}

const char* mediaTrackName(const MediaTrack& track) {
    const char* name = track.path.data();
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' && cursor[1] != '\0') name = cursor + 1;
    }
    return name;
}

uint8_t mediaProgressPercent(uint32_t position, uint32_t size) {
    if (size == 0) return 0;
    if (position >= size) return 100;
    return static_cast<uint8_t>((static_cast<uint64_t>(position) * 100u) / size);
}

void MediaLibrary::clear() {
    for (auto& track : tracks_) track = MediaTrack{};
    size_ = 0;
    selected_ = 0;
    truncated_ = false;
}

bool MediaLibrary::add(const char* path, uint32_t bytes) {
    if (!mediaPathIsMp3(path)) return false;
    const std::size_t length = std::strlen(path);
    if (length == 0 || length >= kMediaPathCapacity) return false;

    MediaTrack candidate;
    std::memcpy(candidate.path.data(), path, length + 1);
    candidate.bytes = bytes;
    if (size_ >= tracks_.size()) {
        truncated_ = true;
        std::size_t largest = 0;
        for (std::size_t index = 1; index < size_; ++index) {
            if (compareTracks(tracks_[largest], tracks_[index]) < 0) largest = index;
        }
        if (compareTracks(candidate, tracks_[largest]) >= 0) return false;
        tracks_[largest] = candidate;
        return true;
    }
    tracks_[size_++] = candidate;
    return true;
}

void MediaLibrary::sort() {
    for (std::size_t index = 1; index < size_; ++index) {
        MediaTrack value = tracks_[index];
        std::size_t insertion = index;
        while (insertion > 0 && compareTracks(value, tracks_[insertion - 1]) < 0) {
            tracks_[insertion] = tracks_[insertion - 1];
            --insertion;
        }
        tracks_[insertion] = value;
    }
    selected_ = 0;
}

const MediaTrack& MediaLibrary::at(std::size_t index) const {
    static const MediaTrack emptyTrack{};
    return index < size_ ? tracks_[index] : emptyTrack;
}

void MediaLibrary::moveSelection(int direction) {
    if (size_ == 0 || direction == 0) return;
    if (direction < 0) {
        selected_ = (selected_ + size_ - 1) % size_;
    } else {
        selected_ = (selected_ + 1) % size_;
    }
}

void MediaLibrary::select(std::size_t index) {
    if (index < size_) selected_ = index;
}

void MediaElapsedClock::start(uint32_t nowMs) {
    accumulatedMs_ = 0;
    activeSinceMs_ = nowMs;
    running_ = true;
}

void MediaElapsedClock::pause(uint32_t nowMs) {
    if (!running_) return;
    accumulatedMs_ += nowMs - activeSinceMs_;
    running_ = false;
}

void MediaElapsedClock::resume(uint32_t nowMs) {
    if (running_) return;
    activeSinceMs_ = nowMs;
    running_ = true;
}

void MediaElapsedClock::stop() {
    accumulatedMs_ = 0;
    activeSinceMs_ = 0;
    running_ = false;
}

uint32_t MediaElapsedClock::elapsed(uint32_t nowMs) const {
    return accumulatedMs_ + (running_ ? nowMs - activeSinceMs_ : 0u);
}

}  // namespace pd
