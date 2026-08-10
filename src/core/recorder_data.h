#pragma once

#include <cstddef>
#include <cstdint>

namespace pd {

inline constexpr std::size_t kRecorderWavHeaderBytes = 44;
inline constexpr std::size_t kRecorderPathCapacity = 40;

enum class RecorderWavParseResult : uint8_t { Valid, Malformed, Unsupported };

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

}  // namespace pd
