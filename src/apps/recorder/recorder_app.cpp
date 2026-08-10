#include "apps/recorder/recorder_app.h"

#include <cstdio>
#include <ctime>

#include "apps/recorder/recorder_app_text.h"
#include "core/localization.h"
#include "core/recorder_data.h"
#include "core/system_context.h"
#include "core/system_settings.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/diagnostics_service.h"
#include "services/recorder_service.h"
#include "services/sd_log_service.h"
#include "ui/localized_font.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

constexpr std::size_t kVisibleRows = 4;
constexpr int16_t kHintY = config::kScreenHeight - theme::kHintHeight;

static_assert(static_cast<uint8_t>(RecorderState::NoCard) == 0u);
static_assert(static_cast<uint8_t>(RecorderState::Empty) == 1u);
static_assert(static_cast<uint8_t>(RecorderState::Ready) == 2u);
static_assert(static_cast<uint8_t>(RecorderState::Recording) == 3u);
static_assert(static_cast<uint8_t>(RecorderState::Playing) == 4u);
static_assert(static_cast<uint8_t>(RecorderState::Unsupported) == 5u);
static_assert(static_cast<uint8_t>(RecorderState::Malformed) == 6u);
static_assert(static_cast<uint8_t>(RecorderState::Error) == 7u);
static_assert(static_cast<uint8_t>(RecorderError::None) == 0u);
static_assert(static_cast<uint8_t>(RecorderError::DeleteFailed) == 22u);

const char* recorderDiagnosticState(RecorderState state) {
    switch (state) {
        case RecorderState::NoCard: return "no-card";
        case RecorderState::Empty: return "empty";
        case RecorderState::Ready: return "ready";
        case RecorderState::Recording: return "recording";
        case RecorderState::Playing: return "playing";
        case RecorderState::Unsupported: return "unsupported";
        case RecorderState::Malformed: return "malformed";
        case RecorderState::Error: return "error";
    }
    return "unknown";
}

const char* recorderDiagnosticError(RecorderError error) {
    switch (error) {
        case RecorderError::None: return "none";
        case RecorderError::NoCard: return "no-card";
        case RecorderError::DirectoryCreateFailed: return "directory-create";
        case RecorderError::DirectoryOpenFailed: return "directory-open";
        case RecorderError::FileOpenFailed: return "file-open";
        case RecorderError::NoFilenameAvailable: return "no-filename";
        case RecorderError::PlaceholderWriteFailed: return "placeholder-write";
        case RecorderError::StorageFull: return "storage-full";
        case RecorderError::MicStartFailed: return "mic-start";
        case RecorderError::MicWakeTimeout: return "mic-wake-timeout";
        case RecorderError::MicQueueFailed: return "mic-queue";
        case RecorderError::MicDrainTimeout: return "mic-drain-timeout";
        case RecorderError::PcmWriteFailed: return "pcm-write";
        case RecorderError::CheckpointFailed: return "checkpoint";
        case RecorderError::FinalizeFailed: return "finalize";
        case RecorderError::PlaybackOpenFailed: return "playback-open";
        case RecorderError::PlaybackReadFailed: return "playback-read";
        case RecorderError::SpeakerStartFailed: return "speaker-start";
        case RecorderError::SpeakerWakeTimeout: return "speaker-wake-timeout";
        case RecorderError::SpeakerQueueFailed: return "speaker-queue";
        case RecorderError::UnsupportedWav: return "unsupported-wav";
        case RecorderError::MalformedWav: return "malformed-wav";
        case RecorderError::DeleteFailed: return "delete";
    }
    return "unknown";
}

void drawHint(M5Canvas& canvas, UiLanguage language, const char* english,
              const char* chinese) {
    canvas.fillRect(0, kHintY, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    setUiFont(canvas, language);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(localized(language, english, chinese), config::kScreenWidth / 2,
                      kHintY + theme::kHintHeight / 2);
}

void formatElapsed(uint32_t elapsedMs, char* output, std::size_t capacity) {
    const uint32_t seconds = elapsedMs / 1000u;
    std::snprintf(output, capacity, "%02lu:%02lu",
                  static_cast<unsigned long>((seconds / 60u) % 100u),
                  static_cast<unsigned long>(seconds % 60u));
}

void formatBytes(uint64_t bytes, char* output, std::size_t capacity) {
    if (bytes < 1024u) {
        std::snprintf(output, capacity, "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024u * 1024u) {
        std::snprintf(output, capacity, "%llu KiB",
                      static_cast<unsigned long long>(bytes / 1024u));
    } else {
        std::snprintf(output, capacity, "%llu MiB",
                      static_cast<unsigned long long>(bytes / (1024u * 1024u)));
    }
}

const char* compatibilityLabel(RecordingCompatibility compatibility, UiLanguage language) {
    switch (compatibility) {
        case RecordingCompatibility::Unknown: return localized(language, "WAV", "WAV");
        case RecordingCompatibility::Valid: return localized(language, "READY", "可播放");
        case RecordingCompatibility::Unsupported:
            return localized(language, "UNSUPPORTED", "不支持");
        case RecordingCompatibility::Malformed: return localized(language, "MALFORMED", "已损坏");
    }
    return localized(language, "WAV", "WAV");
}

const char* displayState(const RecorderSnapshot& snapshot, UiLanguage language) {
    if (snapshot.state == RecorderState::Error) {
        return localizedRecorderErrorLabel(static_cast<uint8_t>(snapshot.error), language);
    }
    return localizedRecorderStateLabel(static_cast<uint8_t>(snapshot.state), language);
}

void drawRecordPage(M5Canvas& canvas, const RecorderSnapshot& snapshot,
                    UiLanguage language) {
    const bool recording = snapshot.state == RecorderState::Recording;
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(recording ? theme::kError : theme::kMuted, theme::kBackground);
    canvas.drawString(displayState(snapshot, language), 6, 17);
    if (recording) {
        canvas.fillCircle(217, 21, 4, theme::kError);
        setTechnicalFont(canvas);
        canvas.setTextDatum(top_right);
        canvas.drawString("REC", 235, 17);
    }

    constexpr int16_t waveformX = 6;
    constexpr int16_t waveformY = 34;
    constexpr int16_t waveformW = 192;
    constexpr int16_t waveformH = 31;
    constexpr int16_t middleY = waveformY + waveformH / 2;
    canvas.drawRect(waveformX, waveformY, waveformW, waveformH, theme::kBorder);
    canvas.drawFastHLine(waveformX + 1, middleY, waveformW - 2, theme::kPanelRaised);
    for (std::size_t index = 0; index < snapshot.waveform.size(); ++index) {
        const int16_t amplitude = snapshot.waveform[index] < 0
                                      ? -snapshot.waveform[index]
                                      : snapshot.waveform[index];
        const int16_t halfHeight = static_cast<int16_t>((amplitude * 14) / 100);
        const int16_t x = waveformX + 2 + static_cast<int16_t>(index * 4);
        canvas.drawFastVLine(x, middleY - halfHeight, halfHeight * 2 + 1,
                             recording ? theme::kError : theme::kSecondary);
    }

    constexpr int16_t meterX = 205;
    constexpr int16_t meterY = 34;
    constexpr int16_t meterH = 31;
    canvas.drawRect(meterX, meterY, 29, meterH, theme::kBorder);
    const int16_t fill = static_cast<int16_t>((meterH - 2) * snapshot.levelPercent / 100u);
    if (fill > 0) {
        canvas.fillRect(meterX + 1, meterY + meterH - 1 - fill, 27, fill,
                        recording ? theme::kError : theme::kPrimary);
    }

    char elapsed[8]{};
    char pcm[20]{};
    char freeBytes[20]{};
    formatElapsed(snapshot.elapsedMs, elapsed, sizeof(elapsed));
    formatBytes(snapshot.pcmBytes, pcm, sizeof(pcm));
    formatBytes(snapshot.freeBytes, freeBytes, sizeof(freeBytes));
    setTechnicalFont(canvas);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString(elapsed, 6, 72);
    canvas.drawString(pcm, 70, 72);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    char freeLine[32]{};
    std::snprintf(freeLine, sizeof(freeLine), "TF %s", freeBytes);
    canvas.drawString(freeLine, 6, 89);

    drawHint(canvas, language, recording ? "ENTER STOP   TAB FILES" : "ENTER REC   TAB FILES",
             recording ? "ENTER 停止   TAB 文件" : "ENTER 录音   TAB 文件");
}

void drawFilesPage(M5Canvas& canvas, const RecorderSnapshot& snapshot,
                   const RecordingLibrary& library, UiLanguage language) {
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    char heading[40]{};
    if (snapshot.entryCount == 0) {
        std::snprintf(heading, sizeof(heading), "%s", displayState(snapshot, language));
    } else {
        std::snprintf(heading, sizeof(heading), "%u/%u%s  %s",
                      static_cast<unsigned>(snapshot.selectedIndex + 1),
                      static_cast<unsigned>(snapshot.entryCount), snapshot.truncated ? "+" : "",
                      compatibilityLabel(snapshot.selectedCompatibility, language));
    }
    canvas.drawString(heading, 6, 17);

    const std::size_t selected = library.selectedIndex();
    std::size_t first = selected > 1 ? selected - 1 : 0;
    if (first + kVisibleRows > library.size()) {
        first = library.size() > kVisibleRows ? library.size() - kVisibleRows : 0;
    }
    canvas.setFont(&fonts::efontCN_14);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    for (std::size_t row = 0; row < kVisibleRows && first + row < library.size(); ++row) {
        const std::size_t index = first + row;
        const bool highlighted = index == selected;
        const int16_t y = 31 + static_cast<int16_t>(row) * 16;
        if (highlighted) canvas.fillRoundRect(4, y - 1, 232, 15, 3, theme::kPanelRaised);
        const RecordingEntry& entry = library.at(index);
        char line[kRecorderPathCapacity + 4]{};
        std::snprintf(line, sizeof(line), "%c %s", highlighted ? '>' : ' ',
                      recordingEntryName(entry));
        canvas.setTextColor(highlighted ? theme::kPrimary : theme::kText,
                            highlighted ? theme::kPanelRaised : theme::kBackground);
        canvas.drawString(line, 6, y);
    }

    const char* state = displayState(snapshot, language);
    setUiFont(canvas, language);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(snapshot.state == RecorderState::Unsupported ||
                                 snapshot.state == RecorderState::Malformed ||
                                 snapshot.state == RecorderState::Error
                             ? theme::kWarning
                             : theme::kMuted,
                        theme::kBackground);
    canvas.drawString(state, 6, 98);
    drawHint(canvas, language, "ENTER PLAY/STOP  d DELETE  TAB REC",
             "ENTER 播放/停止  d 删除  TAB 录音");
}

void drawDeleteConfirm(M5Canvas& canvas, const RecordingLibrary& library,
                       UiLanguage language) {
    canvas.fillRoundRect(4, 25, 232, 83, 6, theme::kPanelRaised);
    canvas.drawRoundRect(4, 25, 232, 83, 6, theme::kError);
    setUiFont(canvas, language);
    canvas.setTextDatum(top_center);
    canvas.setTextColor(theme::kError, theme::kPanelRaised);
    canvas.drawString(localized(language, "DELETE RECORDING?", "删除录音？"),
                      config::kScreenWidth / 2, 33);
    canvas.setFont(&fonts::efontCN_14);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    const char* name = library.empty() ? "" : recordingEntryName(library.selected());
    canvas.drawString(name, config::kScreenWidth / 2, 55);
    setUiFont(canvas, language);
    canvas.setTextColor(theme::kMuted, theme::kPanelRaised);
    canvas.drawString(localized(language, "ENTER DELETE", "ENTER 删除"),
                      config::kScreenWidth / 2, 78);
    drawHint(canvas, language, "BKSP CANCEL", "BKSP 取消");
}

}  // namespace

bool RecorderApp::storageMounted(const SystemContext& context) const {
    return context.sdLog != nullptr && context.sdLog->snapshot().mounted;
}

void RecorderApp::onEnter(SystemContext& context) {
    model_ = RecorderAppModel{};
    loggedSnapshotValid_ = false;
    if (context.recorder == nullptr) return;
    const bool mounted = storageMounted(context);
    const bool scanned = context.recorder->scan(mounted);
    const RecorderSnapshot snapshot = context.recorder->snapshot(context.uptimeMs);
    if (context.diagnostics != nullptr) {
        context.diagnostics->logf("RECORDER scan: mounted=%d ok=%d entries=%u truncated=%d",
                                  mounted ? 1 : 0, scanned ? 1 : 0,
                                  static_cast<unsigned>(snapshot.entryCount),
                                  snapshot.truncated ? 1 : 0);
    }
}

void RecorderApp::onExit(SystemContext& context) {
    model_.exit();
    if (context.recorder == nullptr) return;
    context.recorder->cleanupOnExit(storageMounted(context), context.uptimeMs,
                                    context.volumePercent);
    const RecorderSnapshot snapshot = context.recorder->snapshot(context.uptimeMs);
    if (context.diagnostics != nullptr) {
        context.diagnostics->logf("RECORDER exit: bytes=%lu duration_ms=%lu",
                                  static_cast<unsigned long>(snapshot.completedPcmBytes),
                                  static_cast<unsigned long>(snapshot.completedDurationMs));
    }
    loggedSnapshotValid_ = false;
}

void RecorderApp::onInput(const InputEvent& event, SystemContext& context) {
    if (context.recorder == nullptr) return;
    const RecorderSnapshot snapshot = context.recorder->snapshot(context.uptimeMs);
    const RecorderAppInputState inputState{
        snapshot.entryCount != 0,
        snapshot.state == RecorderState::Recording,
        snapshot.state == RecorderState::Playing,
    };
    switch (model_.handle(event, inputState).effect) {
        case RecorderAppEffect::None: return;
        case RecorderAppEffect::SelectPrevious:
            context.recorder->moveSelection(-1);
            return;
        case RecorderAppEffect::SelectNext:
            context.recorder->moveSelection(1);
            return;
        case RecorderAppEffect::StartRecording:
            context.recorder->startRecording(storageMounted(context), std::time(nullptr),
                                              context.uptimeMs, context.volumePercent);
            return;
        case RecorderAppEffect::StopRecording:
            context.recorder->stopRecording(storageMounted(context), context.uptimeMs,
                                             context.volumePercent);
            return;
        case RecorderAppEffect::StartPlayback:
            context.recorder->startSelectedPlayback(storageMounted(context), context.uptimeMs,
                                                     context.volumePercent);
            return;
        case RecorderAppEffect::StopPlayback:
            context.recorder->stopPlayback(context.volumePercent);
            return;
        case RecorderAppEffect::DeleteSelected:
            context.recorder->deleteSelected(storageMounted(context));
            return;
        case RecorderAppEffect::GoHome:
            context.requestApp(AppId::Launcher);
            return;
        case RecorderAppEffect::Cleanup:
            return;
    }
}

void RecorderApp::update(uint32_t, SystemContext& context) {
    logStateChange(context);
}

void RecorderApp::render(Display& display, const SystemContext& context) {
    const UiLanguage language = context.settings != nullptr
                                    ? context.settings->language
                                    : UiLanguage::English;
    drawStatusBar(display, makeStatusBarData(localized(language, "RECORDER", "录音机"),
                                             context));
    auto& canvas = display.canvas();
    if (context.recorder == nullptr) {
        setUiFont(canvas, language);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString(localized(language, "SERVICE UNAVAILABLE", "服务不可用"),
                          config::kScreenWidth / 2, 60);
        return;
    }
    const RecorderSnapshot snapshot = context.recorder->snapshot(context.uptimeMs);
    switch (model_.page()) {
        case RecorderPage::Record: drawRecordPage(canvas, snapshot, language); break;
        case RecorderPage::Files:
            drawFilesPage(canvas, snapshot, context.recorder->library(), language);
            break;
        case RecorderPage::DeleteConfirm:
            drawDeleteConfirm(canvas, context.recorder->library(), language);
            break;
    }
}

void RecorderApp::logStateChange(SystemContext& context) {
    if (context.recorder == nullptr || context.diagnostics == nullptr) return;
    const RecorderSnapshot snapshot = context.recorder->snapshot(context.uptimeMs);
    const uint8_t state = static_cast<uint8_t>(snapshot.state);
    const uint8_t error = static_cast<uint8_t>(snapshot.error);
    if (loggedSnapshotValid_ && state == loggedState_ && error == loggedError_) return;
    context.diagnostics->logf("RECORDER state: %s error=%s", recorderDiagnosticState(snapshot.state),
                              recorderDiagnosticError(snapshot.error));
    loggedSnapshotValid_ = true;
    loggedState_ = state;
    loggedError_ = error;
}

}  // namespace pd
