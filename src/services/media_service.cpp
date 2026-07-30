#include "services/media_service.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <new>

#include <Arduino.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <FS.h>
#include <M5Cardputer.h>
#include <SD.h>

namespace pd {

constexpr const char* kMusicDirectory = "/Music";
constexpr uint8_t kSpeakerChannel = 0;

class MediaSpeakerOutput final : public AudioOutput {
public:
    bool begin() override { return true; }

    bool ConsumeSample(int16_t sample[2]) override {
        if (bufferIndex_ + 1 < kBufferSamples) {
            buffers_[bufferSlot_][bufferIndex_++] = sample[0];
            buffers_[bufferSlot_][bufferIndex_++] = sample[1];
            return true;
        }
        flush();
        return false;
    }

    void flush() override {
        if (bufferIndex_ == 0) return;
        M5Cardputer.Speaker.playRaw(buffers_[bufferSlot_].data(), bufferIndex_, hertz, true,
                                    1, kSpeakerChannel);
        bufferSlot_ = (bufferSlot_ + 1) % buffers_.size();
        bufferIndex_ = 0;
    }

    bool stop() override {
        flush();
        M5Cardputer.Speaker.stop(kSpeakerChannel);
        return true;
    }

    void silence() { M5Cardputer.Speaker.stop(kSpeakerChannel); }

private:
    // Match M5Unified's official ESP8266Audio adapter. The former half-sized
    // buffers could underrun while Pocket Deck rendered or serviced radios.
    static constexpr std::size_t kBufferSamples = 1536;
    std::array<std::array<int16_t, kBufferSamples>, 3> buffers_{};
    std::size_t bufferSlot_ = 0;
    std::size_t bufferIndex_ = 0;
};

bool makeEntryPath(File& entry, const char* parent, char* output, std::size_t capacity) {
    if (output == nullptr || capacity == 0) return false;
    output[0] = '\0';
    const char* name = entry.name();
    if (name == nullptr || name[0] == '\0') return false;
    const char* leaf = std::strrchr(name, '/');
    leaf = leaf == nullptr ? name : leaf + 1;
    if (leaf[0] == '\0' || leaf[0] == '.' || std::strcmp(leaf, "..") == 0) return false;
    const int written = std::snprintf(output, capacity, "%s/%s", parent, leaf);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
}

const char* mediaPlaybackStateLabel(MediaPlaybackState state) {
    switch (state) {
        case MediaPlaybackState::NoCard: return "NO TF CARD";
        case MediaPlaybackState::Empty: return "NO MP3 FILES";
        case MediaPlaybackState::Ready: return "READY";
        case MediaPlaybackState::Playing: return "PLAYING";
        case MediaPlaybackState::Paused: return "PAUSED";
        case MediaPlaybackState::Error: return "MEDIA ERROR";
    }
    return "UNKNOWN";
}

MediaService::~MediaService() {
    releasePlayback(true);
}

bool MediaService::scan(bool storageMounted) {
    if (!storageMounted) {
        releasePlayback(true);
        library_.clear();
        directoryPath_.fill('\0');
        std::strncpy(directoryPath_.data(), kMusicDirectory, directoryPath_.size() - 1);
        directoryDepth_ = 0;
        hasCurrent_ = false;
        elapsed_.stop();
        setState(MediaPlaybackState::NoCard, "MOUNT TF IN SETTINGS");
        return false;
    }
    if (!SD.exists(kMusicDirectory) && !SD.mkdir(kMusicDirectory)) {
        setState(MediaPlaybackState::Error, "COULD NOT CREATE /Music");
        return false;
    }
    return loadDirectory(kMusicDirectory, 0);
}

bool MediaService::rescan(bool storageMounted) {
    if (state_ == MediaPlaybackState::Playing || state_ == MediaPlaybackState::Paused) {
        return false;
    }
    if (!storageMounted) {
        setState(MediaPlaybackState::NoCard, "MOUNT TF IN SETTINGS");
        return false;
    }
    const char* path = directoryPath_[0] != '\0' ? directoryPath_.data() : kMusicDirectory;
    return loadDirectory(path, directoryDepth_);
}

bool MediaService::loadDirectory(const char* path, uint8_t depth) {
    if (path == nullptr || path[0] != '/' || depth > kMediaMaximumDirectoryDepth) return false;
    std::array<char, kMediaPathCapacity> requested{};
    const std::size_t pathLength = std::strlen(path);
    if (pathLength == 0 || pathLength >= requested.size()) return false;
    std::memcpy(requested.data(), path, pathLength + 1);

    releasePlayback(true);
    library_.clear();
    hasCurrent_ = false;
    elapsed_.stop();
    directoryPath_ = requested;
    directoryDepth_ = depth;

    File directory = SD.open(directoryPath_.data(), FILE_READ);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        setState(MediaPlaybackState::Error, "COULD NOT OPEN FOLDER");
        return false;
    }

    while (true) {
        File entry = directory.openNextFile();
        if (!entry) break;
        std::array<char, kMediaPathCapacity> childPath{};
        if (makeEntryPath(entry, directoryPath_.data(), childPath.data(), childPath.size())) {
            if (entry.isDirectory()) {
                if (directoryDepth_ < kMediaMaximumDirectoryDepth) {
                    library_.addDirectory(childPath.data());
                }
            } else if (mediaPathIsMp3(childPath.data())) {
                library_.add(childPath.data(), static_cast<uint32_t>(entry.size()));
            }
        }
        entry.close();
    }
    directory.close();
    library_.sort();

    if (library_.empty()) {
        setState(MediaPlaybackState::Empty,
                 directoryDepth_ == 0 ? "COPY MP3 TO /Music" : "EMPTY FOLDER");
        return true;
    }
    setState(MediaPlaybackState::Ready, library_.truncated() ? "SHOWING FIRST 64" : nullptr);
    return true;
}

void MediaService::update(uint32_t nowMs) {
    if (state_ != MediaPlaybackState::Playing || decoder_ == nullptr) return;
    if (decoder_->loop()) return;

    const uint32_t position = source_ != nullptr ? source_->getPos() : 0;
    const uint32_t size = source_ != nullptr ? source_->getSize() : 0;
    const bool normalEnd = size > 0 && position >= size;
    const std::size_t finishedIndex = currentIndex_;
    releasePlayback(true);
    elapsed_.stop();
    hasCurrent_ = false;

    if (!normalEnd) {
        setState(MediaPlaybackState::Error, "MP3 DECODE FAILED");
        return;
    }
    if (library_.empty()) {
        setState(MediaPlaybackState::Empty, "COPY MP3 TO /Music");
        return;
    }
    std::size_t next = 0;
    if (!findRelativeTrack(finishedIndex, 1, next)) {
        setState(MediaPlaybackState::Ready);
        return;
    }
    library_.select(next);
    if (!startTrack(next, nowMs)) setState(MediaPlaybackState::Error, "NEXT TRACK FAILED");
}

void MediaService::stop() {
    releasePlayback(true);
    elapsed_.stop();
    hasCurrent_ = false;
    if (library_.empty()) {
        if (state_ != MediaPlaybackState::NoCard) {
            setState(MediaPlaybackState::Empty,
                     directoryDepth_ == 0 ? "COPY MP3 TO /Music" : "EMPTY FOLDER");
        }
    } else {
        setState(MediaPlaybackState::Ready, library_.truncated() ? "SHOWING FIRST 64" : nullptr);
    }
}

void MediaService::moveSelection(int direction) {
    library_.moveSelection(direction);
}

bool MediaService::enterSelectedDirectory() {
    if (library_.empty() || !library_.selected().directory ||
        directoryDepth_ >= kMediaMaximumDirectoryDepth) {
        return false;
    }
    return loadDirectory(library_.selected().path.data(), directoryDepth_ + 1);
}

bool MediaService::goParentDirectory() {
    if (directoryDepth_ == 0) return false;
    std::array<char, kMediaPathCapacity> parent{};
    if (!mediaParentPath(directoryPath_.data(), parent.data(), parent.size())) return false;
    return loadDirectory(parent.data(), directoryDepth_ - 1);
}

bool MediaService::toggleSelected(uint32_t nowMs) {
    if (library_.empty() || library_.selected().directory) return false;
    const std::size_t selected = library_.selectedIndex();
    if (hasCurrent_ && selected == currentIndex_) {
        if (state_ == MediaPlaybackState::Playing) {
            pause(nowMs);
            return true;
        }
        if (state_ == MediaPlaybackState::Paused) {
            resume(nowMs);
            return true;
        }
    }
    return startTrack(selected, nowMs);
}

bool MediaService::playRelative(int direction, uint32_t nowMs) {
    if (library_.empty() || !library_.hasPlayableTrack() || direction == 0) return false;
    const std::size_t base = hasCurrent_ ? currentIndex_ : library_.selectedIndex();
    std::size_t next = 0;
    if (!findRelativeTrack(base, direction, next)) return false;
    library_.select(next);
    return startTrack(next, nowMs);
}

bool MediaService::findRelativeTrack(std::size_t base, int direction,
                                     std::size_t& result) const {
    if (library_.empty() || direction == 0) return false;
    std::size_t index = base < library_.size() ? base : 0;
    for (std::size_t step = 0; step < library_.size(); ++step) {
        index = direction < 0 ? (index + library_.size() - 1) % library_.size()
                              : (index + 1) % library_.size();
        if (!library_.at(index).directory) {
            result = index;
            return true;
        }
    }
    return false;
}

MediaSnapshot MediaService::snapshot(uint32_t nowMs) const {
    MediaSnapshot result;
    result.state = state_;
    result.entryCount = static_cast<uint8_t>(library_.size());
    result.selectedIndex = static_cast<uint8_t>(library_.selectedIndex());
    result.currentIndex = static_cast<uint8_t>(currentIndex_);
    result.hasCurrent = hasCurrent_;
    result.selectedDirectory = !library_.empty() && library_.selected().directory;
    result.truncated = library_.truncated();
    result.directoryDepth = directoryDepth_;
    result.elapsedMs = elapsed_.elapsed(nowMs);
    if (source_ != nullptr) {
        result.progressPercent = mediaProgressPercent(source_->getPos(), source_->getSize());
    }
    result.detail = detail_;
    return result;
}

bool MediaService::startTrack(std::size_t index, uint32_t nowMs) {
    if (index >= library_.size() || library_.at(index).directory) return false;
    releasePlayback(true);
    elapsed_.stop();
    hasCurrent_ = false;

    source_ = new (std::nothrow) AudioFileSourceSD(library_.at(index).path.data());
    output_ = new (std::nothrow) MediaSpeakerOutput();
    decoder_ = new (std::nothrow) AudioGeneratorMP3();
    if (source_ == nullptr || output_ == nullptr || decoder_ == nullptr) {
        releasePlayback(false);
        setState(MediaPlaybackState::Error, "OUT OF MEMORY");
        return false;
    }
    if (!source_->isOpen()) {
        releasePlayback(false);
        setState(MediaPlaybackState::Error, "COULD NOT OPEN MP3");
        return false;
    }
    if (!decoder_->begin(source_, output_)) {
        releasePlayback(false);
        setState(MediaPlaybackState::Error, "MP3 START FAILED");
        return false;
    }

    currentIndex_ = index;
    hasCurrent_ = true;
    library_.select(index);
    elapsed_.start(nowMs);
    setState(MediaPlaybackState::Playing);
    return true;
}

void MediaService::pause(uint32_t nowMs) {
    if (state_ != MediaPlaybackState::Playing) return;
    elapsed_.pause(nowMs);
    if (output_ != nullptr) output_->silence();
    setState(MediaPlaybackState::Paused);
}

void MediaService::resume(uint32_t nowMs) {
    if (state_ != MediaPlaybackState::Paused || decoder_ == nullptr) return;
    elapsed_.resume(nowMs);
    setState(MediaPlaybackState::Playing);
}

void MediaService::releasePlayback(bool stopDecoder) {
    if (decoder_ != nullptr && stopDecoder) {
        // AudioGeneratorMP3::loop() can return false after clearing its running
        // flag without stopping the output. Always call stop() so queued speaker
        // buffers are detached before MediaSpeakerOutput is deleted.
        decoder_->stop();
    } else if (output_ != nullptr) {
        output_->stop();
    }
    if (source_ != nullptr && source_->isOpen()) source_->close();
    delete decoder_;
    delete output_;
    delete source_;
    decoder_ = nullptr;
    output_ = nullptr;
    source_ = nullptr;
}

void MediaService::setState(MediaPlaybackState state, const char* detail) {
    state_ = state;
    detail_.fill('\0');
    if (detail != nullptr) std::strncpy(detail_.data(), detail, detail_.size() - 1);
}

}  // namespace pd
