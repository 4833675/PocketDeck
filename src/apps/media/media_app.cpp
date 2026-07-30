#include "apps/media/media_app.h"

#include <cstdio>
#include <cstring>

#include "core/media_data.h"
#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/diagnostics_service.h"
#include "services/media_service.h"
#include "services/sd_log_service.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

constexpr std::size_t kVisibleRows = 4;
constexpr int16_t kRowsY = 18;
constexpr int16_t kRowHeight = 16;

uint16_t mediaStateColor(MediaPlaybackState state) {
    switch (state) {
        case MediaPlaybackState::Playing: return theme::kPrimary;
        case MediaPlaybackState::Paused: return theme::kWarning;
        case MediaPlaybackState::Error:
        case MediaPlaybackState::NoCard: return theme::kError;
        default: return theme::kMuted;
    }
}

void drawHint(M5Canvas& canvas, const char* text) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString(text, config::kScreenWidth / 2, y + theme::kHintHeight / 2);
}

const char* directoryName(const char* path) {
    if (path == nullptr || path[0] == '\0') return "Music";
    const char* slash = std::strrchr(path, '/');
    return slash != nullptr && slash[1] != '\0' ? slash + 1 : path;
}

void drawEmptyState(M5Canvas& canvas, const MediaSnapshot& snapshot, bool atRoot) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.setTextColor(mediaStateColor(snapshot.state), theme::kBackground);
    canvas.drawString(mediaPlaybackStateLabel(snapshot.state), config::kScreenWidth / 2, 50);
    canvas.setTextSize(1);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(snapshot.detail[0] != '\0' ? snapshot.detail.data() : "TAB TO RESCAN",
                      config::kScreenWidth / 2, 77);
    drawHint(canvas, atRoot ? "TAB RESCAN   DEL HOME" : "TAB RESCAN   DEL UP   G0 HOME");
}

void formatElapsed(uint32_t elapsedMs, char* output, std::size_t capacity) {
    const uint32_t totalSeconds = elapsedMs / 1000u;
    const uint32_t minutes = (totalSeconds / 60u) % 100u;
    const uint32_t seconds = totalSeconds % 60u;
    std::snprintf(output, capacity, "%02lu:%02lu", static_cast<unsigned long>(minutes),
                  static_cast<unsigned long>(seconds));
}

}  // namespace

void MediaApp::onEnter(SystemContext& context) {
    playbackLogValid_ = false;
    if (model_.enter() == MediaAppEffect::Scan) scan(context, true);
}

void MediaApp::onExit(SystemContext& context) {
    if (context.media != nullptr) {
        const bool hadCurrent = context.media->snapshot(context.uptimeMs).hasCurrent;
        context.media->stop();
        if (hadCurrent && context.diagnostics != nullptr) {
            context.diagnostics->log("MEDIA playback released on exit");
        }
    }
    playbackLogValid_ = false;
}

void MediaApp::onInput(const InputEvent& event, SystemContext& context) {
    MediaAppInputState inputState;
    if (context.media != nullptr) {
        inputState.hasEntries = !context.media->library().empty();
        inputState.hasPlayableTrack = context.media->library().hasPlayableTrack();
        inputState.selectedDirectory = inputState.hasEntries &&
                                       context.media->library().selected().directory;
        inputState.atRootDirectory = context.media->atRootDirectory();
    }
    const MediaAppResult result = model_.handle(event, inputState);
    switch (result.effect) {
        case MediaAppEffect::None: return;
        case MediaAppEffect::Scan:
            scan(context, false);
            return;
        case MediaAppEffect::SelectPrevious:
            if (context.media != nullptr) context.media->moveSelection(-1);
            return;
        case MediaAppEffect::SelectNext:
            if (context.media != nullptr) context.media->moveSelection(1);
            return;
        case MediaAppEffect::OpenSelectedDirectory:
            if (context.media != nullptr) {
                const bool opened = context.media->enterSelectedDirectory();
                if (context.diagnostics != nullptr) {
                    const MediaSnapshot snapshot = context.media->snapshot(context.uptimeMs);
                    context.diagnostics->logf(
                        "MEDIA folder enter: ok=%d depth=%u entries=%u truncated=%d",
                        opened ? 1 : 0, static_cast<unsigned>(snapshot.directoryDepth),
                        static_cast<unsigned>(snapshot.entryCount),
                        snapshot.truncated ? 1 : 0);
                }
            }
            return;
        case MediaAppEffect::GoParentDirectory:
            if (context.media != nullptr) {
                const bool opened = context.media->goParentDirectory();
                if (context.diagnostics != nullptr) {
                    const MediaSnapshot snapshot = context.media->snapshot(context.uptimeMs);
                    context.diagnostics->logf(
                        "MEDIA folder up: ok=%d depth=%u entries=%u truncated=%d",
                        opened ? 1 : 0, static_cast<unsigned>(snapshot.directoryDepth),
                        static_cast<unsigned>(snapshot.entryCount),
                        snapshot.truncated ? 1 : 0);
                }
            }
            return;
        case MediaAppEffect::ToggleSelected:
            if (context.media != nullptr) context.media->toggleSelected(context.uptimeMs);
            return;
        case MediaAppEffect::PlayPrevious:
            if (context.media != nullptr) context.media->playRelative(-1, context.uptimeMs);
            return;
        case MediaAppEffect::PlayNext:
            if (context.media != nullptr) context.media->playRelative(1, context.uptimeMs);
            return;
        case MediaAppEffect::AdjustVolume:
            context.requestVolumeDelta(result.volumeDelta);
            return;
        case MediaAppEffect::StopAndGoHome:
            if (context.media != nullptr) context.media->stop();
            context.requestApp(AppId::Launcher);
            return;
    }
}

void MediaApp::update(uint32_t, SystemContext& context) {
    logPlaybackState(context);
}

void MediaApp::render(Display& display, const SystemContext& context) {
    drawStatusBar(display, makeStatusBarData("MEDIA", context));
    auto& canvas = display.canvas();
    if (context.media == nullptr) {
        MediaSnapshot unavailable;
        unavailable.state = MediaPlaybackState::Error;
        std::snprintf(unavailable.detail.data(), unavailable.detail.size(), "SERVICE UNAVAILABLE");
        drawEmptyState(canvas, unavailable, true);
        return;
    }

    const MediaSnapshot snapshot = context.media->snapshot(context.uptimeMs);
    const MediaLibrary& library = context.media->library();
    if (library.empty()) {
        drawEmptyState(canvas, snapshot, context.media->atRootDirectory());
        return;
    }

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
        const bool current = snapshot.hasCurrent && index == snapshot.currentIndex;
        const int16_t y = kRowsY + static_cast<int16_t>(row) * kRowHeight;
        if (highlighted) canvas.fillRoundRect(4, y - 1, 232, 15, 3, theme::kPanelRaised);

        char line[kMediaPathCapacity + 16]{};
        const MediaTrack& item = library.at(index);
        const char marker = current ? (snapshot.state == MediaPlaybackState::Paused ? '=' : '*')
                                    : ' ';
        if (item.directory) {
            std::snprintf(line, sizeof(line), "%c [+] %s", highlighted ? '>' : ' ',
                          mediaTrackName(item));
        } else {
            std::snprintf(line, sizeof(line), "%c%c %s", highlighted ? '>' : ' ', marker,
                          mediaTrackName(item));
        }
        canvas.setTextColor(highlighted ? theme::kPrimary : theme::kText,
                            highlighted ? theme::kPanelRaised : theme::kBackground);
        canvas.drawString(line, 6, y);
    }

    canvas.setFont(&fonts::efontCN_14);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(directoryName(context.media->directoryPath()), 6, 82);
    canvas.setFont(&fonts::Font0);
    canvas.fillRect(187, 81, 49, 14, theme::kBackground);
    char count[20]{};
    std::snprintf(count, sizeof(count), "%u/%u%s", static_cast<unsigned>(selected + 1),
                  static_cast<unsigned>(snapshot.entryCount), snapshot.truncated ? "+" : "");
    canvas.setTextDatum(top_right);
    canvas.setTextColor(theme::kMuted, theme::kBackground);
    canvas.drawString(count, config::kScreenWidth - 6, 84);

    constexpr int16_t progressX = 6;
    constexpr int16_t progressY = 97;
    constexpr int16_t progressW = config::kScreenWidth - 12;
    canvas.drawRect(progressX, progressY, progressW, 6, theme::kBorder);
    const int16_t fill = static_cast<int16_t>((progressW - 2) * snapshot.progressPercent / 100u);
    if (fill > 0) canvas.fillRect(progressX + 1, progressY + 1, fill, 4, theme::kPrimary);

    char elapsed[8]{};
    formatElapsed(snapshot.elapsedMs, elapsed, sizeof(elapsed));
    char status[48]{};
    if ((snapshot.state == MediaPlaybackState::Error ||
         snapshot.state == MediaPlaybackState::Empty) && snapshot.detail[0] != '\0') {
        std::snprintf(status, sizeof(status), "%s", snapshot.detail.data());
        canvas.setTextColor(mediaStateColor(snapshot.state), theme::kBackground);
    } else if (snapshot.state == MediaPlaybackState::Ready) {
        std::snprintf(status, sizeof(status), "READY   VOL %u",
                      static_cast<unsigned>(context.volumePercent));
        canvas.setTextColor(theme::kMuted, theme::kBackground);
    } else {
        std::snprintf(status, sizeof(status), "%s   %3u%%   VOL %u", elapsed,
                      static_cast<unsigned>(snapshot.progressPercent),
                      static_cast<unsigned>(context.volumePercent));
        canvas.setTextColor(theme::kMuted, theme::kBackground);
    }
    canvas.setTextDatum(top_left);
    canvas.drawString(status, 6, 105);
    drawHint(canvas, context.media->atRootDirectory()
                         ? "ENTER OPEN/PLAY  DEL HOME  -/+ VOL"
                         : "ENTER OPEN/PLAY  DEL UP  G0 HOME");
}

void MediaApp::scan(SystemContext& context, bool resetToRoot) {
    if (context.media == nullptr) return;
    const bool mounted = context.sdLog != nullptr && context.sdLog->snapshot().mounted;
    const bool scanned = resetToRoot ? context.media->scan(mounted)
                                     : context.media->rescan(mounted);
    if (context.diagnostics != nullptr) {
        const MediaSnapshot snapshot = context.media->snapshot(context.uptimeMs);
        context.diagnostics->logf("MEDIA scan: ok=%d root=%d entries=%u depth=%u truncated=%d",
                                  scanned ? 1 : 0,
                                  resetToRoot ? 1 : 0,
                                  static_cast<unsigned>(snapshot.entryCount),
                                  static_cast<unsigned>(snapshot.directoryDepth),
                                  snapshot.truncated ? 1 : 0);
    }
}

void MediaApp::logPlaybackState(SystemContext& context) {
    if (context.media == nullptr || context.diagnostics == nullptr) return;
    const MediaSnapshot snapshot = context.media->snapshot(context.uptimeMs);
    const uint8_t state = static_cast<uint8_t>(snapshot.state);
    const bool changed = !playbackLogValid_ || state != loggedState_ ||
                         snapshot.entryCount != loggedEntryCount_ ||
                         snapshot.directoryDepth != loggedDirectoryDepth_ ||
                         snapshot.hasCurrent != loggedHasCurrent_ ||
                         (snapshot.hasCurrent && snapshot.currentIndex != loggedCurrentIndex_);
    if (!changed) return;

    context.diagnostics->logf(
        "MEDIA state: %s depth=%u entries=%u current=%d",
        mediaPlaybackStateLabel(snapshot.state),
        static_cast<unsigned>(snapshot.directoryDepth),
        static_cast<unsigned>(snapshot.entryCount),
        snapshot.hasCurrent ? static_cast<int>(snapshot.currentIndex) : -1);
    if (snapshot.state == MediaPlaybackState::Error && snapshot.detail[0] != '\0') {
        context.diagnostics->logf("MEDIA error: %.38s", snapshot.detail.data());
    }

    playbackLogValid_ = true;
    loggedState_ = state;
    loggedEntryCount_ = snapshot.entryCount;
    loggedDirectoryDepth_ = snapshot.directoryDepth;
    loggedCurrentIndex_ = snapshot.currentIndex;
    loggedHasCurrent_ = snapshot.hasCurrent;
}

}  // namespace pd
