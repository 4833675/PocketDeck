#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kRecorderWavHeaderBytes = 44;
inline constexpr std::size_t kRecorderPathCapacity = 40;
inline constexpr std::size_t kRecordingEntryCapacity = 64;

enum class RecorderWavParseResult : uint8_t { Valid, Malformed, Unsupported };

enum class RecordingCompatibility : uint8_t {
    Unknown,
    Valid,
    Unsupported,
    Malformed,
};

struct RecordingEntry {
    std::array<char, kRecorderPathCapacity> path{};
    uint32_t bytes = 0;
    RecordingCompatibility compatibility = RecordingCompatibility::Unknown;
};

struct RecorderWavMetadata {
    uint32_t dataBytes = 0;
    uint32_t dataOffset = kRecorderWavHeaderBytes;
};

struct RecorderTimestamp {
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
};

bool recorderBuildWavHeader(uint8_t* output, std::size_t capacity, uint32_t dataBytes);
RecorderWavParseResult recorderParseWavHeader(const uint8_t* bytes, std::size_t length,
                                              uint64_t fileSize,
                                              RecorderWavMetadata* metadata);
bool recorderTimestampPath(const RecorderTimestamp& timestamp, char* output,
                           std::size_t capacity);
bool recorderSequentialPath(uint32_t index, char* output, std::size_t capacity);

bool recorderPathIsWav(const char* path);
const char* recordingEntryName(const RecordingEntry& entry);

class RecordingLibrary {
public:
    void clear();
    bool add(const char* path, uint32_t bytes,
             RecordingCompatibility compatibility = RecordingCompatibility::Unknown);
    void sort();
    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    bool truncated() const { return truncated_; }
    std::size_t selectedIndex() const { return selected_; }
    const RecordingEntry& at(std::size_t index) const;
    const RecordingEntry& selected() const { return at(selected_); }
    void moveSelection(int direction);
    void select(std::size_t index);

private:
    std::array<RecordingEntry, kRecordingEntryCapacity> entries_{};
    std::size_t size_ = 0;
    std::size_t selected_ = 0;
    bool truncated_ = false;
};

}  // namespace pd
