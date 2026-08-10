#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <FS.h>

#include "core/recorder_data.h"

namespace pd {

enum class RecorderState : uint8_t {
    NoCard,
    Empty,
    Ready,
    Recording,
    Playing,
    Unsupported,
    Malformed,
    Error,
};

enum class RecorderError : uint8_t {
    None,
    NoCard,
    DirectoryCreateFailed,
    DirectoryOpenFailed,
    FileOpenFailed,
    NoFilenameAvailable,
    PlaceholderWriteFailed,
    StorageFull,
    MicStartFailed,
    MicWakeTimeout,
    MicQueueFailed,
    MicDrainTimeout,
    PcmWriteFailed,
    CheckpointFailed,
    FinalizeFailed,
    PlaybackOpenFailed,
    PlaybackReadFailed,
    SpeakerStartFailed,
    SpeakerWakeTimeout,
    SpeakerQueueFailed,
    UnsupportedWav,
    MalformedWav,
    DeleteFailed,
};

inline constexpr std::size_t kRecorderWaveformColumns = 48;

struct RecorderSnapshot {
    RecorderState state = RecorderState::NoCard;
    RecorderError error = RecorderError::NoCard;
    uint8_t entryCount = 0;
    uint8_t selectedIndex = 0;
    bool truncated = false;
    RecordingCompatibility selectedCompatibility = RecordingCompatibility::Unknown;
    uint32_t elapsedMs = 0;
    uint32_t pcmBytes = 0;
    uint64_t freeBytes = 0;
    std::array<int8_t, kRecorderWaveformColumns> waveform{};
    uint8_t levelPercent = 0;
    uint32_t completedPcmBytes = 0;
    uint32_t completedDurationMs = 0;
};

class RecorderService {
public:
    bool scan(bool storageMounted);
    void update(uint32_t nowMs);
    void moveSelection(int direction);

    bool startRecording(bool storageMounted, int64_t utcEpochSeconds, uint32_t nowMs,
                        uint8_t persistedVolumePercent);
    bool stopRecording(bool storageMounted, uint32_t nowMs,
                       uint8_t persistedVolumePercent);

    bool startSelectedPlayback(bool storageMounted, uint32_t nowMs,
                               uint8_t persistedVolumePercent);
    bool stopPlayback(uint8_t persistedVolumePercent);
    bool deleteSelected(bool storageMounted);

    void cleanupOnExit(bool storageMounted, uint32_t nowMs,
                       uint8_t persistedVolumePercent);

    const RecordingLibrary& library() const { return library_; }
    RecorderSnapshot snapshot(uint32_t nowMs) const;

private:
    enum class CapturePhase : uint8_t { Idle, Waking, Active };
    enum class PlaybackPhase : uint8_t { Idle, Waking, Active };

    static constexpr uint32_t kSampleRate = 16000;
    static constexpr uint32_t kPcmBytesPerSecond = 32000;
    static constexpr uint32_t kCheckpointBytes = 64000;
    static constexpr uint32_t kMicWakeTimeoutMs = 300;
    static constexpr uint32_t kMicDrainTimeoutMs = 1000;
    static constexpr uint32_t kMicDiscardTimeoutMs = 400;
    static constexpr uint32_t kSpeakerWakeTimeoutMs = 300;
    static constexpr uint32_t kPlaybackCompletionMarginMs = 8;
    static constexpr uint8_t kPlaybackChannel = 7;
    static constexpr uint8_t kInvalidBufferIndex = 0xFF;
    static constexpr std::size_t kCaptureSamples = 1024;
    static constexpr std::size_t kPlaybackSamples = 1024;
    static constexpr std::size_t kShortPlaybackFallbackSamples = 256;
    static constexpr int16_t kCaptureSentinel = 0x5A5A;

    bool scanStorage(bool storageMounted, bool updateIdleState);
    bool ensureRecordingsDirectory();
    void refreshFreeBytes(bool storageMounted);
    bool chooseRecordingPath(int64_t utcEpochSeconds);

    bool primeCapture(uint32_t nowMs);
    bool queueCapture(uint8_t bufferIndex);
    bool processCapture(uint32_t nowMs, bool allowRequeue);
    bool releaseCompletedCapture(std::size_t hardwareDepth);
    bool refillCapture(uint32_t nowMs);
    bool writeCompletedCapture(uint8_t bufferIndex);
    bool checkpointRecording();
    bool drainCaptureNormally();
    void discardCaptureBounded();
    void resetCaptureQueue();
    int16_t* captureData(uint8_t bufferIndex);
    bool captureBufferUntouched(uint8_t bufferIndex) const;
    void updateWaveform(const int16_t* samples);
    bool finishRecording(bool storageMounted, uint32_t nowMs,
                         uint8_t persistedVolumePercent, bool drainNormally,
                         RecorderError terminalError);
    bool finalizeRecordingFile();

    bool updatePlayback(uint32_t nowMs);
    bool queueNextPlaybackChunk();
    bool primePlayback();
    bool queuePlaybackBuffer(uint8_t bufferIndex, std::size_t sampleCount,
                             std::size_t hardwareDepthBefore);
    int16_t* playbackData(uint8_t bufferIndex);
    int findFreePlaybackBuffer() const;
    void releaseConsumedPlaybackBuffers(std::size_t hardwareDepth);
    bool stopPlaybackInternal(uint8_t persistedVolumePercent, bool restoreSpeaker);
    void finishPlaybackNaturally();
    void resetPlaybackQueue();
    void setSelectedCompatibility(RecordingCompatibility compatibility);

    void stopSpeakerForCapture();
    bool restoreSpeaker(uint8_t persistedVolumePercent);
    void setIdleState();
    void setError(RecorderError error);

    RecordingLibrary library_{};
    std::array<RecordingCompatibility, kRecordingEntryCapacity> compatibility_{};
    RecorderState state_ = RecorderState::NoCard;
    RecorderError error_ = RecorderError::NoCard;
    uint64_t freeBytes_ = 0;
    uint64_t recordingFreeBudget_ = 0;
    uint32_t activityStartedMs_ = 0;
    uint32_t pcmBytes_ = 0;
    uint32_t completedPcmBytes_ = 0;
    uint32_t completedDurationMs_ = 0;
    uint32_t nextCheckpointBytes_ = kCheckpointBytes;
    uint8_t persistedVolumePercent_ = 0;
    bool activeStorageMounted_ = false;

    File recordingFile_{};
    std::array<char, kRecorderPathCapacity> recordingPath_{};

    // Exactly two capture arrays. A caller-owned array remains immutable while
    // its corresponding M5Unified Mic descriptor is present in captureFifo_.
    std::array<int16_t, 1024> captureBufferA_{};
    std::array<int16_t, 1024> captureBufferB_{};
    std::array<uint8_t, 2> captureFifo_{};
    std::array<bool, 2> captureQueuedFlags_{};
    uint8_t captureFifoHead_ = 0;
    uint8_t captureQueued_ = 0;
    uint8_t captureObservedDepth_ = 0;
    CapturePhase capturePhase_ = CapturePhase::Idle;
    uint32_t captureWakeDeadlineMs_ = 0;
    uint8_t captureWakingBuffer_ = kInvalidBufferIndex;

    std::array<int8_t, kRecorderWaveformColumns> waveform_{};
    uint8_t levelPercent_ = 0;

    File playbackFile_{};
    uint32_t playbackRemainingBytes_ = 0;
    uint32_t playbackTotalBytes_ = 0;
    std::array<int16_t, 1024> playbackBufferA_{};
    std::array<int16_t, 1024> playbackBufferB_{};
    std::array<int16_t, 1024> playbackBufferC_{};
    std::array<uint8_t, 2> playbackFifo_{};
    std::array<bool, 3> playbackQueuedFlags_{};
    uint8_t playbackFifoHead_ = 0;
    uint8_t playbackQueued_ = 0;
    PlaybackPhase playbackPhase_ = PlaybackPhase::Idle;
    uint32_t playbackWakeDeadlineMs_ = 0;
    uint32_t playbackWakingStartedMs_ = 0;
    uint16_t playbackWakingSamples_ = 0;
    uint8_t playbackWakingBuffer_ = kInvalidBufferIndex;
};

}  // namespace pd
