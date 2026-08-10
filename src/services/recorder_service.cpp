#include "services/recorder_service.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>

#include "pocket_deck_config.h"

namespace pd {
namespace {

constexpr char kRecordingsDirectory[] = "/Recordings";
constexpr std::time_t kPlausibleEpoch = 1704067200;  // 2024-01-01 UTC
constexpr uint32_t kMaximumWavDataBytes = 0xFFFFFFFFu - 36u;

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

uint8_t rawVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    return static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255u) / 100u);
}

bool makeDirectEntryPath(File& entry, char* output, std::size_t capacity) {
    if (output == nullptr || capacity == 0) return false;
    output[0] = '\0';
    const char* name = entry.name();
    if (name == nullptr || name[0] == '\0') return false;
    const char* leaf = std::strrchr(name, '/');
    leaf = leaf == nullptr ? name : leaf + 1;
    if (leaf[0] == '\0' || leaf[0] == '.') return false;
    const int written = std::snprintf(output, capacity, "%s/%s", kRecordingsDirectory,
                                      leaf);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
}

}  // namespace

bool RecorderService::scan(bool storageMounted) {
    if (state_ == RecorderState::Recording || state_ == RecorderState::Playing ||
        recordingFile_ || playbackFile_) {
        return false;
    }
    return scanStorage(storageMounted, true);
}

bool RecorderService::scanStorage(bool storageMounted, bool updateIdleState) {
    library_.clear();
    compatibility_.fill(RecordingCompatibility::Unknown);
    if (!storageMounted) {
        refreshFreeBytes(false);
        if (updateIdleState) {
            state_ = RecorderState::NoCard;
            error_ = RecorderError::NoCard;
        }
        return false;
    }
    refreshFreeBytes(true);
    if (!ensureRecordingsDirectory()) {
        if (updateIdleState) setError(RecorderError::DirectoryCreateFailed);
        return false;
    }

    File directory = SD.open(kRecordingsDirectory, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        if (updateIdleState) setError(RecorderError::DirectoryOpenFailed);
        return false;
    }

    while (true) {
        File entry = directory.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            std::array<char, kRecorderPathCapacity> path{};
            if (makeDirectEntryPath(entry, path.data(), path.size()) &&
                recorderPathIsWav(path.data())) {
                const std::size_t fileBytes = entry.size();
                const uint32_t boundedBytes =
                    fileBytes > std::numeric_limits<uint32_t>::max()
                        ? std::numeric_limits<uint32_t>::max()
                        : static_cast<uint32_t>(fileBytes);
                library_.add(path.data(), boundedBytes);
            }
        }
        entry.close();
    }
    directory.close();
    library_.sort();
    refreshFreeBytes(true);
    if (updateIdleState) setIdleState();
    return true;
}

bool RecorderService::ensureRecordingsDirectory() {
    if (SD.exists(kRecordingsDirectory)) return true;
    return SD.mkdir(kRecordingsDirectory);
}

void RecorderService::refreshFreeBytes(bool storageMounted) {
    if (!storageMounted) {
        freeBytes_ = 0;
        return;
    }
    const uint64_t total = SD.totalBytes();
    const uint64_t used = SD.usedBytes();
    freeBytes_ = used < total ? total - used : 0;
}

bool RecorderService::chooseRecordingPath(int64_t utcEpochSeconds) {
    recordingPath_.fill('\0');
    if (utcEpochSeconds >= static_cast<int64_t>(kPlausibleEpoch)) {
        const int64_t localEpoch =
            utcEpochSeconds + static_cast<int64_t>(config::kLocalUtcOffsetSeconds);
        const std::time_t localTime = static_cast<std::time_t>(localEpoch);
        std::tm local{};
        if (gmtime_r(&localTime, &local) != nullptr) {
            const RecorderTimestamp timestamp{
                static_cast<uint16_t>(local.tm_year + 1900),
                static_cast<uint8_t>(local.tm_mon + 1),
                static_cast<uint8_t>(local.tm_mday),
                static_cast<uint8_t>(local.tm_hour),
                static_cast<uint8_t>(local.tm_min),
                static_cast<uint8_t>(local.tm_sec),
            };
            if (recorderTimestampPath(timestamp, recordingPath_.data(),
                                      recordingPath_.size()) &&
                !SD.exists(recordingPath_.data())) {
                return true;
            }
        }
    }

    for (uint32_t index = 0; index <= 9999; ++index) {
        if (!recorderSequentialPath(index, recordingPath_.data(),
                                    recordingPath_.size())) {
            break;
        }
        if (!SD.exists(recordingPath_.data())) return true;
    }
    recordingPath_.fill('\0');
    return false;
}

bool RecorderService::startRecording(bool storageMounted, int64_t utcEpochSeconds,
                                     uint32_t nowMs,
                                     uint8_t persistedVolumePercent) {
    if (state_ == RecorderState::Recording || recordingFile_) return false;
    persistedVolumePercent_ = persistedVolumePercent;
    activeStorageMounted_ = storageMounted;
    if (!storageMounted) {
        state_ = RecorderState::NoCard;
        error_ = RecorderError::NoCard;
        return false;
    }

    if (state_ == RecorderState::Playing || playbackFile_) {
        if (!stopPlaybackInternal(persistedVolumePercent_, false)) {
            setError(RecorderError::SpeakerStartFailed);
            return false;
        }
    }

    stopSpeakerForCapture();
    M5Cardputer.Mic.end();
    if (!M5Cardputer.Mic.begin() || !M5Cardputer.Mic.isRunning()) {
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::MicStartFailed);
        return false;
    }

    refreshFreeBytes(true);
    if (!ensureRecordingsDirectory()) {
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::DirectoryCreateFailed);
        return false;
    }
    refreshFreeBytes(true);
    recordingFreeBudget_ = freeBytes_;
    if (!chooseRecordingPath(utcEpochSeconds)) {
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::NoFilenameAvailable);
        return false;
    }
    if (recordingFreeBudget_ < kRecorderWavHeaderBytes) {
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::StorageFull);
        return false;
    }

    recordingFile_ = SD.open(recordingPath_.data(), FILE_WRITE);
    if (!recordingFile_) {
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::FileOpenFailed);
        return false;
    }

    std::array<uint8_t, kRecorderWavHeaderBytes> header{};
    const bool headerReady = recorderBuildWavHeader(header.data(), header.size(), 0);
    const std::size_t headerWritten =
        headerReady ? recordingFile_.write(header.data(), header.size()) : 0;
    if (!headerReady || headerWritten != header.size()) {
        recordingFile_.close();
        // This path was proven absent immediately before FILE_WRITE. Only a
        // failed placeholder creation may remove a recorder-created file.
        SD.remove(recordingPath_.data());
        recordingPath_.fill('\0');
        M5Cardputer.Mic.end();
        restoreSpeaker(persistedVolumePercent_);
        setError(RecorderError::PlaceholderWriteFailed);
        return false;
    }
    recordingFreeBudget_ -= headerWritten;
    // Make the zero-length canonical header durable before Mic owns any caller
    // buffer. Later 64,000-byte checkpoints advance this crash-safe boundary.
    recordingFile_.flush();

    pcmBytes_ = 0;
    nextCheckpointBytes_ = kCheckpointBytes;
    activityStartedMs_ = nowMs;
    waveform_.fill(0);
    levelPercent_ = 0;
    captureBufferA_.fill(0);
    captureBufferB_.fill(0);
    resetCaptureQueue();
    state_ = RecorderState::Recording;
    error_ = RecorderError::None;

    // M5Unified's first record() fills the descriptor selected by _rec_flip.
    // A second immediate call can wait forever if the Mic task never wakes, so
    // initial capture queues only A and waits for observed activity.
    if (!primeCapture(nowMs)) {
        return finishRecording(storageMounted, nowMs, persistedVolumePercent_, false,
                               RecorderError::MicQueueFailed);
    }
    return true;
}

bool RecorderService::stopRecording(bool storageMounted, uint32_t nowMs,
                                    uint8_t persistedVolumePercent) {
    if (state_ != RecorderState::Recording || !recordingFile_) return false;
    persistedVolumePercent_ = persistedVolumePercent;
    return finishRecording(storageMounted, nowMs, persistedVolumePercent_, true,
                           RecorderError::None);
}

void RecorderService::update(uint32_t nowMs) {
    if (state_ == RecorderState::Recording) {
        if (!processCapture(nowMs, true)) {
            const RecorderError terminal =
                error_ == RecorderError::None ? RecorderError::MicQueueFailed : error_;
            finishRecording(activeStorageMounted_, nowMs, persistedVolumePercent_, false,
                            terminal);
        }
        return;
    }
    if (state_ == RecorderState::Playing) updatePlayback(nowMs);
}

bool RecorderService::primeCapture(uint32_t nowMs) {
    // Initial start and every full-drain restart queue A only. Queueing B before
    // a nonzero isRecording() observation can block forever in _rec_raw.
    if (captureQueued_ != 0 || !queueCapture(0)) return false;
    capturePhase_ = CapturePhase::Waking;
    captureObservedDepth_ = 0;
    captureWakeDeadlineMs_ = nowMs + kMicWakeTimeoutMs;
    return true;
}

bool RecorderService::queueCapture(uint8_t bufferIndex) {
    if (bufferIndex >= captureQueuedFlags_.size() ||
        captureQueuedFlags_[bufferIndex] || captureQueued_ >= 2 ||
        !M5Cardputer.Mic.isRunning()) {
        return false;
    }

    // Invariant: record() is called only when our FIFO proves one of
    // M5Unified's two internal descriptors is free. The array is not written or
    // reused again until a later observed depth decrease releases this FIFO row.
    if (!M5Cardputer.Mic.record(captureData(bufferIndex), kCaptureSamples,
                               kSampleRate, false)) {
        return false;
    }
    const uint8_t tail = static_cast<uint8_t>((captureFifoHead_ + captureQueued_) % 2);
    captureFifo_[tail] = bufferIndex;
    captureQueuedFlags_[bufferIndex] = true;
    ++captureQueued_;
    return true;
}

bool RecorderService::processCapture(uint32_t nowMs, bool allowRequeue) {
    if (captureQueued_ == 0) return true;
    if (!M5Cardputer.Mic.isRunning()) {
        setError(RecorderError::MicStartFailed);
        return false;
    }

    const std::size_t hardwareDepth = M5Cardputer.Mic.isRecording();
    if (hardwareDepth > captureQueued_ || hardwareDepth > 2) {
        setError(RecorderError::MicQueueFailed);
        return false;
    }

    if (capturePhase_ == CapturePhase::Waking) {
        if (hardwareDepth == 0) {
            if (deadlineReached(nowMs, captureWakeDeadlineMs_)) {
                setError(RecorderError::MicWakeTimeout);
                return false;
            }
            // Immediate zero is the documented _is_recording wake race, not a
            // completed block. No SD access or second record() is allowed yet.
            return true;
        }

        // A nonzero depth proves the Mic task woke. Reconcile any completed
        // stale FIFO rows before queueing B or reusing their arrays.
        if (!releaseCompletedCapture(hardwareDepth)) return false;
        capturePhase_ = CapturePhase::Active;
        captureObservedDepth_ = captureQueued_;
        if (!allowRequeue) return true;
        return refillCapture(nowMs);
    }

    if (!releaseCompletedCapture(hardwareDepth)) return false;
    captureObservedDepth_ = captureQueued_;

    if (!allowRequeue) {
        if (captureQueued_ == 0) capturePhase_ = CapturePhase::Idle;
        return true;
    }
    return refillCapture(nowMs);
}

bool RecorderService::releaseCompletedCapture(std::size_t hardwareDepth) {
    if (hardwareDepth > captureQueued_ || hardwareDepth > 2) {
        setError(RecorderError::MicQueueFailed);
        return false;
    }
    while (captureQueued_ > hardwareDepth) {
        const uint8_t bufferIndex = captureFifo_[captureFifoHead_];
        captureFifoHead_ = static_cast<uint8_t>((captureFifoHead_ + 1) % 2);
        --captureQueued_;
        captureQueuedFlags_[bufferIndex] = false;
        if (!writeCompletedCapture(bufferIndex)) return false;
    }
    return true;
}

bool RecorderService::refillCapture(uint32_t nowMs) {
    // An SD write can outlast the other captured block. Re-sample at most twice
    // (there are exactly two descriptors) and write every newly released FIFO
    // row before selecting an array for record().
    for (uint8_t pass = 0; pass < 2; ++pass) {
        const std::size_t hardwareDepth = M5Cardputer.Mic.isRecording();
        if (hardwareDepth > captureQueued_ || hardwareDepth > 2) {
            setError(RecorderError::MicQueueFailed);
            return false;
        }
        if (hardwareDepth == captureQueued_) break;
        if (!releaseCompletedCapture(hardwareDepth)) return false;
    }

    if (captureQueued_ == 0) {
        // Once both descriptors drain, the task can sleep again. Re-prime A
        // only and require the complete Waking handshake before queueing B.
        if (!primeCapture(nowMs)) setError(RecorderError::MicQueueFailed);
        return capturePhase_ == CapturePhase::Waking;
    }
    if (captureQueued_ >= 2) {
        captureObservedDepth_ = captureQueued_;
        return true;
    }

    const std::size_t hardwareDepthBefore = M5Cardputer.Mic.isRecording();
    if (hardwareDepthBefore > captureQueued_ || hardwareDepthBefore > 2) {
        setError(RecorderError::MicQueueFailed);
        return false;
    }
    if (hardwareDepthBefore < captureQueued_) {
        if (!releaseCompletedCapture(hardwareDepthBefore)) return false;
        if (captureQueued_ == 0) {
            if (!primeCapture(nowMs)) setError(RecorderError::MicQueueFailed);
            return capturePhase_ == CapturePhase::Waking;
        }
    }

    const uint8_t freeBuffer = captureQueuedFlags_[0] ? 1 : 0;
    if (!queueCapture(freeBuffer)) {
        setError(RecorderError::MicQueueFailed);
        return false;
    }
    const std::size_t hardwareDepthAfter = M5Cardputer.Mic.isRecording();
    if (hardwareDepthAfter > captureQueued_ || hardwareDepthAfter > 2) {
        setError(RecorderError::MicQueueFailed);
        return false;
    }
    captureObservedDepth_ = static_cast<uint8_t>(hardwareDepthAfter);
    if (hardwareDepthBefore == 0 || hardwareDepthAfter < captureQueued_) {
        // The previous descriptor drained around record(). A zero can be the
        // wake race, and a nonzero lower depth can leave a stale FIFO row ahead
        // of the new pointer. Waking preserves both until a later nonzero depth
        // lets processCapture reconcile FIFO ownership safely.
        capturePhase_ = CapturePhase::Waking;
        captureWakeDeadlineMs_ = nowMs + kMicWakeTimeoutMs;
    }
    return true;
}

bool RecorderService::writeCompletedCapture(uint8_t bufferIndex) {
    const int16_t* samples = captureData(bufferIndex);
    updateWaveform(samples);
    constexpr std::size_t blockBytes = kCaptureSamples * sizeof(int16_t);
    if (!recordingFile_) {
        setError(RecorderError::PcmWriteFailed);
        return false;
    }
    if (pcmBytes_ > kMaximumWavDataBytes - blockBytes) {
        setError(RecorderError::StorageFull);
        return false;
    }
    if (recordingFreeBudget_ < blockBytes) {
        setError(RecorderError::StorageFull);
        return false;
    }

    const uint32_t before = pcmBytes_;
    const std::size_t written = recordingFile_.write(
        reinterpret_cast<const uint8_t*>(samples), blockBytes);
    const std::size_t evenWritten = written & ~static_cast<std::size_t>(1);
    pcmBytes_ += static_cast<uint32_t>(evenWritten);
    recordingFreeBudget_ =
        written < recordingFreeBudget_ ? recordingFreeBudget_ - written : 0;

    bool checkpointOk = true;
    while (before < nextCheckpointBytes_ && pcmBytes_ >= nextCheckpointBytes_) {
        if (!checkpointRecording()) {
            checkpointOk = false;
            break;
        }
        if (nextCheckpointBytes_ >
            std::numeric_limits<uint32_t>::max() - kCheckpointBytes) {
            nextCheckpointBytes_ = std::numeric_limits<uint32_t>::max();
            break;
        }
        nextCheckpointBytes_ += kCheckpointBytes;
    }

    if (written != blockBytes || (written & 1u) != 0u) {
        // Only complete int16_t bytes are declared. If the filesystem accepted
        // one stray odd byte, finalization seeks to the even logical end and the
        // canonical header deliberately excludes that trailing byte.
        setError(RecorderError::PcmWriteFailed);
        return false;
    }
    if (!checkpointOk) {
        setError(RecorderError::CheckpointFailed);
        return false;
    }
    return true;
}

bool RecorderService::checkpointRecording() {
    std::array<uint8_t, kRecorderWavHeaderBytes> header{};
    if (!recordingFile_ ||
        !recorderBuildWavHeader(header.data(), header.size(), pcmBytes_) ||
        !recordingFile_.seek(0)) {
        return false;
    }
    if (recordingFile_.write(header.data(), header.size()) != header.size()) return false;
    if (!recordingFile_.seek(static_cast<uint32_t>(kRecorderWavHeaderBytes + pcmBytes_))) {
        return false;
    }
    recordingFile_.flush();
    return true;
}

bool RecorderService::drainCaptureNormally() {
    const uint32_t deadline = millis() + kMicDrainTimeoutMs;
    while (captureQueued_ > 0) {
        const uint32_t nowMs = millis();
        if (deadlineReached(nowMs, deadline)) {
            setError(RecorderError::MicDrainTimeout);
            return false;
        }
        if (!processCapture(nowMs, false)) return false;
        if (captureQueued_ > 0) delay(1);
    }
    return true;
}

void RecorderService::discardCaptureBounded() {
    if (captureQueued_ == 0) return;
    const uint32_t deadline = millis() + kMicDiscardTimeoutMs;
    bool sawActive = capturePhase_ == CapturePhase::Active;
    while (M5Cardputer.Mic.isRunning()) {
        const std::size_t depth = M5Cardputer.Mic.isRecording();
        if (depth > 0) sawActive = true;
        if (depth == 0 && sawActive) break;
        const uint32_t nowMs = millis();
        if (deadlineReached(nowMs, deadline)) break;
        delay(1);
    }
}

bool RecorderService::finishRecording(bool storageMounted, uint32_t nowMs,
                                      uint8_t persistedVolumePercent,
                                      bool drainNormally,
                                      RecorderError terminalError) {
    (void)nowMs;
    RecorderError outcome = terminalError;
    if (drainNormally && outcome == RecorderError::None) {
        if (!drainCaptureNormally()) {
            outcome = error_ == RecorderError::None ? RecorderError::MicDrainTimeout : error_;
            discardCaptureBounded();
        }
    } else {
        discardCaptureBounded();
    }

    std::array<char, kRecorderPathCapacity> emptyFailedPath{};
    const bool removeEmptyFailedStart =
        pcmBytes_ == 0 && recordingPath_[0] != '\0' &&
        (outcome == RecorderError::MicQueueFailed ||
         outcome == RecorderError::MicWakeTimeout);
    if (removeEmptyFailedStart) emptyFailedPath = recordingPath_;

    M5Cardputer.Mic.end();
    const bool finalized = finalizeRecordingFile();
    if (removeEmptyFailedStart) {
        // No PCM was ever declared and the path is still provably the unique
        // file created by this start attempt. Do not leave an empty recording.
        SD.remove(emptyFailedPath.data());
    }
    if (!finalized && outcome == RecorderError::None) outcome = RecorderError::FinalizeFailed;
    completedPcmBytes_ = pcmBytes_;
    completedDurationMs_ = static_cast<uint32_t>(
        (static_cast<uint64_t>(completedPcmBytes_) * 1000u) / kPcmBytesPerSecond);
    resetCaptureQueue();

    const bool speakerReady = restoreSpeaker(persistedVolumePercent);
    if (!speakerReady && outcome == RecorderError::None) {
        outcome = RecorderError::SpeakerStartFailed;
    }

    const bool scanned = scanStorage(storageMounted, true);
    if (outcome != RecorderError::None) {
        setError(outcome);
    }
    return outcome == RecorderError::None && finalized && speakerReady && scanned;
}

bool RecorderService::finalizeRecordingFile() {
    if (!recordingFile_) {
        recordingPath_.fill('\0');
        return true;
    }

    std::array<uint8_t, kRecorderWavHeaderBytes> header{};
    bool success = recorderBuildWavHeader(header.data(), header.size(), pcmBytes_);
    if (!recordingFile_.seek(0)) {
        success = false;
    } else if (recordingFile_.write(header.data(), header.size()) != header.size()) {
        success = false;
    }
    if (!recordingFile_.seek(static_cast<uint32_t>(kRecorderWavHeaderBytes + pcmBytes_))) {
        success = false;
    }
    recordingFile_.flush();
    recordingFile_.close();
    recordingPath_.fill('\0');
    return success;
}

void RecorderService::resetCaptureQueue() {
    captureFifo_.fill(0);
    captureQueuedFlags_.fill(false);
    captureFifoHead_ = 0;
    captureQueued_ = 0;
    captureObservedDepth_ = 0;
    capturePhase_ = CapturePhase::Idle;
    captureWakeDeadlineMs_ = 0;
}

int16_t* RecorderService::captureData(uint8_t bufferIndex) {
    return bufferIndex == 0 ? captureBufferA_.data() : captureBufferB_.data();
}

void RecorderService::updateWaveform(const int16_t* samples) {
    if (samples == nullptr) return;
    int32_t blockPeak = 0;
    for (std::size_t column = 0; column < waveform_.size(); ++column) {
        const std::size_t begin = (column * kCaptureSamples) / waveform_.size();
        const std::size_t end = ((column + 1) * kCaptureSamples) / waveform_.size();
        int32_t signedPeak = 0;
        for (std::size_t index = begin; index < end; ++index) {
            const int32_t value = samples[index];
            const int32_t magnitude = value < 0 ? -value : value;
            const int32_t peakMagnitude = signedPeak < 0 ? -signedPeak : signedPeak;
            if (magnitude > peakMagnitude) signedPeak = value;
            if (magnitude > blockPeak) blockPeak = magnitude;
        }
        int32_t normalized = (signedPeak * 100) / 32767;
        if (normalized < -100) normalized = -100;
        if (normalized > 100) normalized = 100;
        waveform_[column] = static_cast<int8_t>(normalized);
    }
    uint32_t level = static_cast<uint32_t>(blockPeak) * 100u / 32767u;
    if (level > 100) level = 100;
    levelPercent_ = static_cast<uint8_t>(level);
}

bool RecorderService::startSelectedPlayback(bool storageMounted, uint32_t nowMs,
                                            uint8_t persistedVolumePercent) {
    if (state_ == RecorderState::Recording || state_ == RecorderState::Playing ||
        recordingFile_ || playbackFile_) {
        return false;
    }
    persistedVolumePercent_ = persistedVolumePercent;
    activeStorageMounted_ = storageMounted;
    if (!storageMounted) {
        state_ = RecorderState::NoCard;
        error_ = RecorderError::NoCard;
        return false;
    }
    if (library_.empty()) {
        state_ = RecorderState::Empty;
        error_ = RecorderError::None;
        return false;
    }

    refreshFreeBytes(true);
    playbackFile_ = SD.open(library_.selected().path.data(), FILE_READ);
    if (!playbackFile_) {
        setError(RecorderError::PlaybackOpenFailed);
        return false;
    }

    std::array<uint8_t, kRecorderWavHeaderBytes> header{};
    const std::size_t headerBytes = playbackFile_.read(header.data(), header.size());
    RecorderWavMetadata metadata{};
    RecorderWavParseResult parsed = RecorderWavParseResult::Malformed;
    if (headerBytes == header.size()) {
        parsed = recorderParseWavHeader(header.data(), header.size(), playbackFile_.size(),
                                        &metadata);
    }
    if (parsed != RecorderWavParseResult::Valid) {
        playbackFile_.close();
        if (parsed == RecorderWavParseResult::Unsupported) {
            setSelectedCompatibility(RecordingCompatibility::Unsupported);
            state_ = RecorderState::Unsupported;
            error_ = RecorderError::UnsupportedWav;
        } else {
            setSelectedCompatibility(RecordingCompatibility::Malformed);
            state_ = RecorderState::Malformed;
            error_ = RecorderError::MalformedWav;
        }
        return false;
    }
    setSelectedCompatibility(RecordingCompatibility::Valid);
    if (!playbackFile_.seek(metadata.dataOffset)) {
        playbackFile_.close();
        setError(RecorderError::PlaybackReadFailed);
        return false;
    }

    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.stop(kPlaybackChannel);
    M5Cardputer.Speaker.end();
    resetPlaybackQueue();
    if (!restoreSpeaker(persistedVolumePercent_)) {
        playbackFile_.close();
        setError(RecorderError::SpeakerStartFailed);
        return false;
    }

    playbackRemainingBytes_ = metadata.dataBytes;
    playbackTotalBytes_ = metadata.dataBytes;
    activityStartedMs_ = nowMs;
    state_ = RecorderState::Playing;
    error_ = RecorderError::None;
    if (!updatePlayback(nowMs)) return false;
    return true;
}

bool RecorderService::updatePlayback(uint32_t nowMs) {
    if (state_ != RecorderState::Playing || !playbackFile_ ||
        !M5Cardputer.Speaker.isRunning()) {
        stopPlaybackInternal(persistedVolumePercent_, true);
        setError(RecorderError::SpeakerStartFailed);
        return false;
    }

    const std::size_t hardwareDepth = M5Cardputer.Speaker.isPlaying(kPlaybackChannel);
    if (hardwareDepth > playbackQueued_ || hardwareDepth > 2) {
        stopPlaybackInternal(persistedVolumePercent_, true);
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }

    if (playbackPhase_ == PlaybackPhase::Idle) {
        if (playbackRemainingBytes_ == 0) {
            finishPlaybackNaturally();
            return true;
        }
        if (!primePlayback(nowMs)) {
            const RecorderError terminal = error_;
            stopPlaybackInternal(persistedVolumePercent_, true);
            setError(terminal);
            return false;
        }
        return true;
    }

    if (playbackPhase_ == PlaybackPhase::Waking) {
        if (hardwareDepth == 0) {
            if (!deadlineReached(nowMs, playbackWakeDeadlineMs_)) return true;
            stopPlaybackInternal(persistedVolumePercent_, true);
            setError(RecorderError::SpeakerWakeTimeout);
            return false;
        }
        // As with Mic startup, do not release or refill the first pointer until
        // a nonzero channel depth proves the Speaker task accepted ownership.
        releaseConsumedPlaybackBuffers(hardwareDepth);
        playbackPhase_ = PlaybackPhase::Active;
    } else {
        releaseConsumedPlaybackBuffers(hardwareDepth);
        if (hardwareDepth == 0 && playbackQueued_ == 0) {
            if (playbackRemainingBytes_ == 0) {
                finishPlaybackNaturally();
                return true;
            }
            // A fully drained channel is re-primed with exactly one pointer and
            // must pass the bounded Waking handshake again before queue growth.
            if (!primePlayback(nowMs)) {
                const RecorderError terminal = error_;
                stopPlaybackInternal(persistedVolumePercent_, true);
                setError(terminal);
                return false;
            }
            return true;
        }
    }

    while (playbackPhase_ == PlaybackPhase::Active && playbackQueued_ < 2 &&
           playbackRemainingBytes_ > 0) {
        if (!queueNextPlaybackChunk(nowMs)) {
            const RecorderError terminal = error_;
            stopPlaybackInternal(persistedVolumePercent_, true);
            setError(terminal);
            return false;
        }
    }

    if (playbackRemainingBytes_ == 0 && playbackQueued_ == 0) {
        finishPlaybackNaturally();
    }
    return true;
}

bool RecorderService::primePlayback(uint32_t nowMs) {
    if (playbackQueued_ != 0 || playbackRemainingBytes_ == 0 ||
        !queueNextPlaybackChunk(nowMs)) {
        if (error_ == RecorderError::None) setError(RecorderError::SpeakerQueueFailed);
        return false;
    }
    playbackPhase_ = PlaybackPhase::Waking;
    playbackWakeDeadlineMs_ = nowMs + kSpeakerWakeTimeoutMs;
    return true;
}

bool RecorderService::queueNextPlaybackChunk(uint32_t nowMs) {
    if (playbackPhase_ == PlaybackPhase::Waking) {
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }

    // SD reads can outlast a playing chunk. Reconcile once before choosing a
    // destination, and again immediately before playRaw(), so a pointer whose
    // descriptor completed during the read is never mistaken for still queued.
    std::size_t hardwareDepth = M5Cardputer.Speaker.isPlaying(kPlaybackChannel);
    if (hardwareDepth > playbackQueued_ || hardwareDepth > 2) {
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }
    releaseConsumedPlaybackBuffers(hardwareDepth);

    const int freeBuffer = findFreePlaybackBuffer();
    if (freeBuffer < 0 || playbackRemainingBytes_ == 0) {
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }
    const std::size_t request =
        playbackRemainingBytes_ < kPlaybackSamples * sizeof(int16_t)
            ? playbackRemainingBytes_
            : kPlaybackSamples * sizeof(int16_t);
    if (request == 0 || (request & 1u) != 0u) {
        setError(RecorderError::PlaybackReadFailed);
        return false;
    }
    const std::size_t read = playbackFile_.read(
        reinterpret_cast<uint8_t*>(playbackData(static_cast<uint8_t>(freeBuffer))),
        request);
    if (read != request || (read & 1u) != 0u) {
        setError(RecorderError::PlaybackReadFailed);
        return false;
    }

    hardwareDepth = M5Cardputer.Speaker.isPlaying(kPlaybackChannel);
    if (hardwareDepth > playbackQueued_ || hardwareDepth > 2) {
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }
    releaseConsumedPlaybackBuffers(hardwareDepth);
    if (!queuePlaybackBuffer(static_cast<uint8_t>(freeBuffer), read / 2,
                             hardwareDepth, nowMs)) {
        setError(RecorderError::SpeakerQueueFailed);
        return false;
    }
    playbackRemainingBytes_ -= static_cast<uint32_t>(read);
    return true;
}

bool RecorderService::queuePlaybackBuffer(uint8_t bufferIndex,
                                          std::size_t sampleCount,
                                          std::size_t hardwareDepthBefore,
                                          uint32_t nowMs) {
    if (bufferIndex >= playbackQueuedFlags_.size() || sampleCount == 0 ||
        playbackQueuedFlags_[bufferIndex] || playbackQueued_ >= 2 ||
        hardwareDepthBefore > playbackQueued_ || hardwareDepthBefore >= 2 ||
        !M5Cardputer.Speaker.isRunning()) {
        return false;
    }

    // M5Unified retains this pointer in one of two descriptors. playRaw() can
    // return true even when begin/task creation failed, so nonzero input and an
    // independently running Speaker are prerequisites; its return is not the
    // sole startup proof. The FIFO prevents reuse until depth later decreases.
    const bool accepted = M5Cardputer.Speaker.playRaw(
        playbackData(bufferIndex), sampleCount, kSampleRate, false, 1,
        kPlaybackChannel, false);
    if (!accepted || !M5Cardputer.Speaker.isRunning()) return false;

    const uint8_t tail = static_cast<uint8_t>((playbackFifoHead_ + playbackQueued_) % 2);
    playbackFifo_[tail] = bufferIndex;
    playbackQueuedFlags_[bufferIndex] = true;
    ++playbackQueued_;
    const std::size_t hardwareDepthAfter =
        M5Cardputer.Speaker.isPlaying(kPlaybackChannel);
    if (hardwareDepthBefore == 0 || hardwareDepthAfter == 0) {
        // A queue into an idle channel has the same ambiguous immediate-zero
        // window as initial playback. Protect the newly queued pointer until a
        // later nonzero observation, even if stale FIFO rows precede it.
        playbackPhase_ = PlaybackPhase::Waking;
        playbackWakeDeadlineMs_ = nowMs + kSpeakerWakeTimeoutMs;
    }
    return true;
}

void RecorderService::releaseConsumedPlaybackBuffers(std::size_t hardwareDepth) {
    while (playbackQueued_ > hardwareDepth) {
        const uint8_t bufferIndex = playbackFifo_[playbackFifoHead_];
        playbackFifoHead_ = static_cast<uint8_t>((playbackFifoHead_ + 1) % 2);
        --playbackQueued_;
        if (bufferIndex < playbackQueuedFlags_.size()) {
            playbackQueuedFlags_[bufferIndex] = false;
        }
    }
}

int RecorderService::findFreePlaybackBuffer() const {
    for (std::size_t index = 0; index < playbackQueuedFlags_.size(); ++index) {
        if (!playbackQueuedFlags_[index]) return static_cast<int>(index);
    }
    return -1;
}

int16_t* RecorderService::playbackData(uint8_t bufferIndex) {
    if (bufferIndex == 0) return playbackBufferA_.data();
    if (bufferIndex == 1) return playbackBufferB_.data();
    return playbackBufferC_.data();
}

bool RecorderService::stopPlayback(uint8_t persistedVolumePercent) {
    if (state_ != RecorderState::Playing && !playbackFile_) return false;
    persistedVolumePercent_ = persistedVolumePercent;
    const bool restored = stopPlaybackInternal(persistedVolumePercent_, true);
    refreshFreeBytes(activeStorageMounted_);
    if (restored) {
        setIdleState();
    } else {
        setError(RecorderError::SpeakerStartFailed);
    }
    return restored;
}

bool RecorderService::stopPlaybackInternal(uint8_t persistedVolumePercent,
                                           bool restoreAfterStop) {
    M5Cardputer.Speaker.stop(kPlaybackChannel);
    // end() synchronously clears both M5Unified descriptors. It must happen
    // before any queued playback array is marked free or Mic can take ownership.
    M5Cardputer.Speaker.end();
    resetPlaybackQueue();
    playbackRemainingBytes_ = 0;
    playbackTotalBytes_ = 0;
    if (playbackFile_) playbackFile_.close();
    return !restoreAfterStop || restoreSpeaker(persistedVolumePercent);
}

void RecorderService::finishPlaybackNaturally() {
    // The hardware depth is zero, so no Speaker descriptor retains a buffer.
    if (playbackFile_) playbackFile_.close();
    resetPlaybackQueue();
    playbackRemainingBytes_ = 0;
    playbackTotalBytes_ = 0;
    refreshFreeBytes(activeStorageMounted_);
    setIdleState();
}

void RecorderService::resetPlaybackQueue() {
    playbackFifo_.fill(0);
    playbackQueuedFlags_.fill(false);
    playbackFifoHead_ = 0;
    playbackQueued_ = 0;
    playbackPhase_ = PlaybackPhase::Idle;
    playbackWakeDeadlineMs_ = 0;
}

void RecorderService::setSelectedCompatibility(
    RecordingCompatibility compatibility) {
    if (library_.empty()) return;
    const std::size_t selected = library_.selectedIndex();
    if (selected < compatibility_.size()) compatibility_[selected] = compatibility;
}

bool RecorderService::deleteSelected(bool storageMounted) {
    if (state_ == RecorderState::Recording || state_ == RecorderState::Playing ||
        recordingFile_ || playbackFile_) {
        return false;
    }
    if (!storageMounted) {
        state_ = RecorderState::NoCard;
        error_ = RecorderError::NoCard;
        return false;
    }
    if (library_.empty() || !recorderPathIsWav(library_.selected().path.data())) {
        return false;
    }
    // The App/model owns the separate confirmation screen. This API performs
    // exactly one confirmed deletion and never removes a directory or sibling.
    if (!SD.remove(library_.selected().path.data())) {
        setError(RecorderError::DeleteFailed);
        return false;
    }
    return scanStorage(true, true);
}

void RecorderService::cleanupOnExit(bool storageMounted, uint32_t nowMs,
                                    uint8_t persistedVolumePercent) {
    persistedVolumePercent_ = persistedVolumePercent;
    if (state_ == RecorderState::Recording || recordingFile_) {
        finishRecording(storageMounted, nowMs, persistedVolumePercent_, true,
                        RecorderError::None);
        return;
    }

    M5Cardputer.Mic.end();
    stopPlaybackInternal(persistedVolumePercent_, true);
    if (storageMounted) {
        setIdleState();
    } else {
        state_ = RecorderState::NoCard;
        error_ = RecorderError::NoCard;
        freeBytes_ = 0;
    }
}

void RecorderService::moveSelection(int direction) {
    if (state_ == RecorderState::Recording || state_ == RecorderState::Playing) return;
    library_.moveSelection(direction);
    if (state_ == RecorderState::Unsupported || state_ == RecorderState::Malformed) {
        setIdleState();
    }
}

RecorderSnapshot RecorderService::snapshot(uint32_t nowMs) const {
    RecorderSnapshot result;
    result.state = state_;
    result.error = error_;
    result.entryCount = static_cast<uint8_t>(library_.size());
    result.selectedIndex = static_cast<uint8_t>(library_.selectedIndex());
    result.truncated = library_.truncated();
    if (!library_.empty() && library_.selectedIndex() < compatibility_.size()) {
        result.selectedCompatibility = compatibility_[library_.selectedIndex()];
    }
    if (state_ == RecorderState::Recording || state_ == RecorderState::Playing) {
        result.elapsedMs = nowMs - activityStartedMs_;
    }
    result.pcmBytes = pcmBytes_;
    result.freeBytes = freeBytes_;
    result.waveform = waveform_;
    result.levelPercent = levelPercent_;
    result.completedPcmBytes = completedPcmBytes_;
    result.completedDurationMs = completedDurationMs_;
    return result;
}

void RecorderService::stopSpeakerForCapture() {
    M5Cardputer.Speaker.stop(kPlaybackChannel);
    // Speaker.end() is the deterministic pointer-detach barrier before Mic or
    // capture buffers can take over the shared M5Unified audio path.
    M5Cardputer.Speaker.end();
    resetPlaybackQueue();
}

bool RecorderService::restoreSpeaker(uint8_t persistedVolumePercent) {
    const bool begun = M5Cardputer.Speaker.begin();
    if (!begun || !M5Cardputer.Speaker.isRunning()) return false;
    M5Cardputer.Speaker.setVolume(rawVolume(persistedVolumePercent));
    return true;
}

void RecorderService::setIdleState() {
    state_ = library_.empty() ? RecorderState::Empty : RecorderState::Ready;
    error_ = RecorderError::None;
}

void RecorderService::setError(RecorderError error) {
    state_ = RecorderState::Error;
    error_ = error;
}

}  // namespace pd
