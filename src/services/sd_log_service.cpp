#include "services/sd_log_service.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

namespace pd {
namespace {

constexpr int8_t kSdSckPin = 40;
constexpr int8_t kSdMisoPin = 39;
constexpr int8_t kSdMosiPin = 14;
constexpr int8_t kSdCsPin = 12;
constexpr uint32_t kSdFrequency = 25000000;
constexpr uint32_t kRotateBytes = 512u * 1024u;
constexpr const char* kPreviousLogPath = "/PocketDeck/ble-prev.log";
constexpr std::time_t kPlausibleEpoch = 1704067200;  // 2024-01-01 UTC

}  // namespace

bool SdLogService::begin() {
    return mount(false);
}

bool SdLogService::remount() {
    unmount();
    return mount(false);
}

bool SdLogService::formatCard() {
    unmount();
    snapshot_.state = SdLogState::Formatting;

    // formatIfMissing lets the Arduino FatFs layer run f_mkfs when the card has
    // no usable FAT volume (including a typical exFAT card on this old core).
    // For an already valid FAT card, clearing the root provides quick-format
    // semantics without unsafe raw-sector writes.
    if (!mount(true)) return false;
    snapshot_.state = SdLogState::Formatting;
    if (!eraseDirectory("/", 0)) {
        setError("Could not erase all files");
        return false;
    }
    if (!ensureDirectory()) {
        setError("Could not create log folder");
        return false;
    }

    snapshot_.linesWritten = 0;
    snapshot_.error.fill('\0');
    snapshot_.state = SdLogState::Ready;
    ready_ = true;
    refreshUsage();
    return true;
}

bool SdLogService::beginSession(const char* firmwareVersion, const char* resetReason) {
    char message[128];
    std::snprintf(message, sizeof(message), "=== Pocket Deck v%s boot reset=%s ===",
                  firmwareVersion != nullptr ? firmwareVersion : "?",
                  resetReason != nullptr ? resetReason : "unknown");
    return append(message);
}

bool SdLogService::append(const char* message) {
    if (!ready_ || snapshot_.state != SdLogState::Ready || message == nullptr) return false;
    if (!rotateIfNeeded()) return false;

    File file = SD.open(kLogPath, FILE_APPEND);
    if (!file) {
        setError("Could not open ble.log");
        return false;
    }

    char timestamp[32];
    const std::time_t now = std::time(nullptr);
    if (now >= kPlausibleEpoch) {
        std::tm utc{};
        gmtime_r(&now, &utc);
        std::snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                      utc.tm_min, utc.tm_sec);
    } else {
        std::snprintf(timestamp, sizeof(timestamp), "TIME-UNSYNCED");
    }

    std::array<char, 256> line{};
    std::snprintf(line.data(), line.size(), "%s up=%010lu %s\n", timestamp,
                  static_cast<unsigned long>(millis()), message);
    const size_t expected = std::strlen(line.data());
    const size_t written = file.print(line.data());
    file.flush();
    file.close();
    if (written != expected) {
        setError("Short write to ble.log");
        return false;
    }

    ++snapshot_.linesWritten;
    if ((snapshot_.linesWritten & 0x0Fu) == 0) refreshUsage();
    return true;
}

void SdLogService::diagnosticsSink(void* context, const char* message) {
    if (context == nullptr) return;
    static_cast<SdLogService*>(context)->append(message);
}

bool SdLogService::mount(bool formatIfMissing) {
    ready_ = false;
    snapshot_.mounted = false;
    snapshot_.cardBytes = 0;
    snapshot_.usedBytes = 0;
    snapshot_.error.fill('\0');
    snapshot_.state = SdLogState::Unavailable;

    SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
    if (!SD.begin(kSdCsPin, SPI, kSdFrequency, "/sd", 5, formatIfMissing)) {
        setError(formatIfMissing ? "No card or format failed" : "No FAT card mounted",
                 SdLogState::Unavailable);
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        SD.end();
        setError("No TF card detected", SdLogState::Unavailable);
        return false;
    }

    snapshot_.mounted = true;
    snapshot_.cardBytes = SD.cardSize();
    if (!ensureDirectory()) {
        setError("Could not create log folder");
        return false;
    }
    snapshot_.state = SdLogState::Ready;
    ready_ = true;
    refreshUsage();
    return true;
}

void SdLogService::unmount() {
    ready_ = false;
    if (snapshot_.mounted) SD.end();
    snapshot_.mounted = false;
    snapshot_.state = SdLogState::Unavailable;
}

bool SdLogService::ensureDirectory() {
    if (SD.exists(kDirectory)) return true;
    return SD.mkdir(kDirectory);
}

bool SdLogService::eraseDirectory(const char* path, uint8_t depth) {
    if (depth > 8) return false;
    File directory = SD.open(path);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        return false;
    }

    bool success = true;
    while (true) {
        File entry = directory.openNextFile();
        if (!entry) break;
        std::array<char, 192> entryPath{};
        const char* pathValue = entry.path();
        if (pathValue != nullptr) {
            std::strncpy(entryPath.data(), pathValue, entryPath.size() - 1);
        }
        const bool isDirectory = entry.isDirectory();
        entry.close();

        if (entryPath[0] == '\0') {
            success = false;
            continue;
        }
        if (isDirectory) {
            if (!eraseDirectory(entryPath.data(), static_cast<uint8_t>(depth + 1)) ||
                !SD.rmdir(entryPath.data())) {
                success = false;
            }
        } else if (!SD.remove(entryPath.data())) {
            success = false;
        }
    }
    directory.close();
    return success;
}

bool SdLogService::rotateIfNeeded() {
    File current = SD.open(kLogPath, FILE_READ);
    if (!current) return true;
    const size_t size = current.size();
    current.close();
    if (size < kRotateBytes) return true;

    if (SD.exists(kPreviousLogPath) && !SD.remove(kPreviousLogPath)) {
        setError("Could not remove old log");
        return false;
    }
    if (!SD.rename(kLogPath, kPreviousLogPath)) {
        setError("Could not rotate ble.log");
        return false;
    }
    return true;
}

void SdLogService::refreshUsage() {
    if (!snapshot_.mounted) return;
    snapshot_.usedBytes = SD.usedBytes();
}

void SdLogService::setError(const char* message, SdLogState state) {
    snapshot_.state = state;
    snapshot_.error.fill('\0');
    if (message != nullptr) {
        std::strncpy(snapshot_.error.data(), message, snapshot_.error.size() - 1);
    }
    ready_ = false;
}

}  // namespace pd
