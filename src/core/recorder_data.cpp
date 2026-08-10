#include "core/recorder_data.h"

#include <cstdio>
#include <cstring>

namespace pd {
namespace {

constexpr uint32_t kRecorderSampleRate = 16000;
constexpr uint32_t kRecorderByteRate = 32000;
constexpr uint32_t kRecorderMaximumDataBytes = 0xFFFFFFFFu - 36u;
constexpr std::size_t kTimestampPathBytes = 36;
constexpr std::size_t kSequentialPathBytes = 25;
constexpr char kRecordingsDirectory[] = "/Recordings/";

void writeLe16(uint8_t* output, uint16_t value) {
    output[0] = static_cast<uint8_t>(value & 0xFFu);
    output[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

void writeLe32(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value & 0xFFu);
    output[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    output[2] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
    output[3] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

uint16_t readLe16(const uint8_t* input) {
    return static_cast<uint16_t>(static_cast<uint16_t>(input[0]) |
                                 (static_cast<uint16_t>(input[1]) << 8u));
}

uint32_t readLe32(const uint8_t* input) {
    return static_cast<uint32_t>(static_cast<uint32_t>(input[0]) |
                                 (static_cast<uint32_t>(input[1]) << 8u) |
                                 (static_cast<uint32_t>(input[2]) << 16u) |
                                 (static_cast<uint32_t>(input[3]) << 24u));
}

bool hasTag(const uint8_t* bytes, std::size_t offset, const char* tag) {
    return std::memcmp(bytes + offset, tag, 4) == 0;
}

bool isLeapYear(uint16_t year) {
    return year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u);
}

bool isValidTimestamp(const RecorderTimestamp& timestamp) {
    if (timestamp.year == 0 || timestamp.month < 1 || timestamp.month > 12 ||
        timestamp.hour > 23 || timestamp.minute > 59 || timestamp.second > 59) {
        return false;
    }
    constexpr uint8_t daysByMonth[] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    uint8_t days = daysByMonth[timestamp.month - 1];
    if (timestamp.month == 2 && isLeapYear(timestamp.year)) ++days;
    return timestamp.day >= 1 && timestamp.day <= days;
}

bool formatPath(char* output, std::size_t capacity, std::size_t requiredBytes,
                const char* format, uint32_t first, uint32_t second, uint32_t third,
                uint32_t fourth, uint32_t fifth, uint32_t sixth) {
    if (output == nullptr || capacity < requiredBytes) return false;
    const int written = std::snprintf(output, capacity, format, first, second, third, fourth,
                                      fifth, sixth);
    return written >= 0 && static_cast<std::size_t>(written) + 1 == requiredBytes;
}

char asciiLower(char value) {
    if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
    return value;
}

bool entryIsNewer(const RecordingEntry& lhs, const RecordingEntry& rhs) {
    return std::strcmp(lhs.path.data(), rhs.path.data()) > 0;
}

}  // namespace

bool recorderBuildWavHeader(uint8_t* output, std::size_t capacity, uint32_t dataBytes) {
    if (output == nullptr || capacity < kRecorderWavHeaderBytes ||
        dataBytes > kRecorderMaximumDataBytes || dataBytes % 2u != 0u) {
        return false;
    }

    std::memset(output, 0, kRecorderWavHeaderBytes);
    std::memcpy(output, "RIFF", 4);
    writeLe32(output + 4, 36u + dataBytes);
    std::memcpy(output + 8, "WAVEfmt ", 8);
    writeLe32(output + 16, 16);
    writeLe16(output + 20, 1);
    writeLe16(output + 22, 1);
    writeLe32(output + 24, kRecorderSampleRate);
    writeLe32(output + 28, kRecorderByteRate);
    writeLe16(output + 32, 2);
    writeLe16(output + 34, 16);
    std::memcpy(output + 36, "data", 4);
    writeLe32(output + 40, dataBytes);
    return true;
}

RecorderWavParseResult recorderParseWavHeader(const uint8_t* bytes, std::size_t length,
                                              uint64_t fileSize,
                                              RecorderWavMetadata* metadata) {
    if (bytes == nullptr || length < kRecorderWavHeaderBytes ||
        fileSize < kRecorderWavHeaderBytes || !hasTag(bytes, 0, "RIFF") ||
        !hasTag(bytes, 8, "WAVE") || !hasTag(bytes, 12, "fmt ") ||
        !hasTag(bytes, 36, "data") || readLe32(bytes + 16) != 16) {
        return RecorderWavParseResult::Malformed;
    }

    const uint32_t dataBytes = readLe32(bytes + 40);
    if (readLe32(bytes + 4) != static_cast<uint64_t>(36u) + dataBytes ||
        static_cast<uint64_t>(dataBytes) > fileSize - kRecorderWavHeaderBytes) {
        return RecorderWavParseResult::Malformed;
    }

    if (readLe16(bytes + 20) != 1 || readLe16(bytes + 22) != 1 ||
        readLe32(bytes + 24) != kRecorderSampleRate ||
        readLe32(bytes + 28) != kRecorderByteRate || readLe16(bytes + 32) != 2 ||
        readLe16(bytes + 34) != 16) {
        return RecorderWavParseResult::Unsupported;
    }
    if (dataBytes % 2u != 0u) return RecorderWavParseResult::Malformed;

    if (metadata != nullptr) {
        metadata->dataBytes = dataBytes;
        metadata->dataOffset = kRecorderWavHeaderBytes;
    }
    return RecorderWavParseResult::Valid;
}

bool recorderTimestampPath(const RecorderTimestamp& timestamp, char* output,
                           std::size_t capacity) {
    if (!isValidTimestamp(timestamp)) return false;
    return formatPath(output, capacity, kTimestampPathBytes,
                      "/Recordings/REC_%04u%02u%02u_%02u%02u%02u.WAV", timestamp.year,
                      timestamp.month, timestamp.day, timestamp.hour, timestamp.minute,
                      timestamp.second);
}

bool recorderSequentialPath(uint32_t index, char* output, std::size_t capacity) {
    if (index > 9999) return false;
    return formatPath(output, capacity, kSequentialPathBytes, "/Recordings/REC_%04u.WAV",
                      index, 0, 0, 0, 0, 0);
}

bool recorderPathIsWav(const char* path) {
    if (path == nullptr || std::strncmp(path, kRecordingsDirectory,
                                        sizeof(kRecordingsDirectory) - 1) != 0) {
        return false;
    }

    const char* leaf = path + sizeof(kRecordingsDirectory) - 1;
    if (*leaf == '\0') return false;
    for (const char* cursor = leaf; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') return false;
    }

    const std::size_t leafLength = std::strlen(leaf);
    return leafLength > 4 && leaf[leafLength - 4] == '.' &&
           asciiLower(leaf[leafLength - 3]) == 'w' &&
           asciiLower(leaf[leafLength - 2]) == 'a' &&
           asciiLower(leaf[leafLength - 1]) == 'v';
}

const char* recordingEntryName(const RecordingEntry& entry) {
    const char* name = entry.path.data();
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' && cursor[1] != '\0') name = cursor + 1;
    }
    return name;
}

void RecordingLibrary::clear() {
    for (auto& entry : entries_) entry = RecordingEntry{};
    size_ = 0;
    selected_ = 0;
    truncated_ = false;
}

bool RecordingLibrary::add(const char* path, uint32_t bytes,
                           RecordingCompatibility compatibility) {
    if (!recorderPathIsWav(path)) return false;
    const std::size_t length = std::strlen(path);
    if (length == 0 || length >= kRecorderPathCapacity) return false;

    RecordingEntry candidate;
    std::memcpy(candidate.path.data(), path, length + 1);
    candidate.bytes = bytes;
    candidate.compatibility = compatibility;

    if (size_ < entries_.size()) {
        entries_[size_++] = candidate;
        return true;
    }

    truncated_ = true;
    std::size_t oldest = 0;
    for (std::size_t index = 1; index < size_; ++index) {
        if (entryIsNewer(entries_[oldest], entries_[index])) oldest = index;
    }
    if (!entryIsNewer(candidate, entries_[oldest])) return false;
    entries_[oldest] = candidate;
    return true;
}

void RecordingLibrary::sort() {
    for (std::size_t index = 1; index < size_; ++index) {
        RecordingEntry value = entries_[index];
        std::size_t insertion = index;
        while (insertion > 0 && entryIsNewer(value, entries_[insertion - 1])) {
            entries_[insertion] = entries_[insertion - 1];
            --insertion;
        }
        entries_[insertion] = value;
    }
    selected_ = 0;
}

const RecordingEntry& RecordingLibrary::at(std::size_t index) const {
    static const RecordingEntry emptyEntry{};
    return index < size_ ? entries_[index] : emptyEntry;
}

void RecordingLibrary::moveSelection(int direction) {
    if (size_ == 0 || direction == 0) return;
    if (direction < 0) {
        selected_ = (selected_ + size_ - 1) % size_;
    } else {
        selected_ = (selected_ + 1) % size_;
    }
}

void RecordingLibrary::select(std::size_t index) {
    if (index < size_) selected_ = index;
}

}  // namespace pd
