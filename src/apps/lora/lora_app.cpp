#include "apps/lora/lora_app.h"

#include <cstdio>

#include "core/lora_data.h"
#include "core/system_context.h"
#include "drivers/display.h"
#include "pocket_deck_config.h"
#include "services/lora_service.h"
#include "ui/status_bar.h"
#include "ui/theme.h"

namespace pd {
namespace {

constexpr int16_t kProfileY = 18;
constexpr int16_t kHistoryY = 29;
constexpr int16_t kHistoryStep = 10;
constexpr int16_t kQualityY = 90;
constexpr int16_t kDraftY = 102;
constexpr std::size_t kVisibleMessageBytes = 34;
constexpr std::size_t kVisibleDraftBytes = 36;

const char* visibleTail(const char* text, std::size_t length, std::size_t limit) {
    return text + (length > limit ? length - limit : 0);
}

uint16_t stateColor(LoRaRadioState state) {
    switch (state) {
        case LoRaRadioState::Listening: return theme::kPrimary;
        case LoRaRadioState::Initializing:
        case LoRaRadioState::Transmitting: return theme::kWarning;
        case LoRaRadioState::Unavailable:
        case LoRaRadioState::Error: return theme::kError;
    }
    return theme::kMuted;
}

void formatState(const LoRaData& data, char* output, std::size_t capacity) {
    switch (data.state()) {
        case LoRaRadioState::Unavailable:
            std::snprintf(output, capacity, "RADIO NOT FOUND");
            return;
        case LoRaRadioState::Initializing:
            std::snprintf(output, capacity, "STARTING");
            return;
        case LoRaRadioState::Listening:
            std::snprintf(output, capacity, "LISTENING");
            return;
        case LoRaRadioState::Transmitting:
            std::snprintf(output, capacity, "BUSY");
            return;
        case LoRaRadioState::Error:
            std::snprintf(output, capacity, "ERROR %d", data.lastStatusCode());
            return;
    }
}

void drawHint(M5Canvas& canvas) {
    const int16_t y = config::kScreenHeight - theme::kHintHeight;
    canvas.fillRect(0, y, config::kScreenWidth, theme::kHintHeight, theme::kPanel);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(theme::kMuted, theme::kPanel);
    canvas.drawString("ENTER SEND   FN+` HOME", config::kScreenWidth / 2,
                      y + theme::kHintHeight / 2);
}

}  // namespace

void LoRaApp::onEnter(SystemContext& context) {
    if (model_.enter() == LoRaAppEffect::EnsureStarted && context.lora != nullptr) {
        context.lora->ensureStarted(context.diagnostics);
    }
}

void LoRaApp::onExit(SystemContext& context) {
    if (model_.exit() == LoRaAppEffect::ClearDraft && context.lora != nullptr) {
        context.lora->clearDraft();
    }
}

void LoRaApp::onInput(const InputEvent& event, SystemContext& context) {
    const LoRaData* data = context.lora != nullptr ? &context.lora->data() : nullptr;
    const LoRaAppResult result = model_.handle(
        event, data != nullptr ? data->state() : LoRaRadioState::Unavailable,
        data == nullptr || data->draftEmpty(), context.uptimeMs);
    switch (result.effect) {
        case LoRaAppEffect::None:
        case LoRaAppEffect::EnsureStarted:
        case LoRaAppEffect::ClearDraft: return;
        case LoRaAppEffect::AppendDraft:
            if (context.lora != nullptr) context.lora->appendDraft(result.character);
            return;
        case LoRaAppEffect::EraseDraft:
            if (context.lora != nullptr) context.lora->eraseDraft();
            return;
        case LoRaAppEffect::RequestTransmit:
            if (context.lora != nullptr && !context.lora->requestTransmit()) {
                model_.rejectSend(context.uptimeMs);
            }
            return;
        case LoRaAppEffect::GoHome:
            context.requestApp(AppId::Launcher);
            return;
    }
}

void LoRaApp::update(uint32_t, SystemContext&) {}

void LoRaApp::render(Display& display, const SystemContext& context) {
    drawStatusBar(display, makeStatusBarData("LORA", context));
    auto& canvas = display.canvas();
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);

    if (context.lora == nullptr) {
        canvas.setTextColor(theme::kError, theme::kBackground);
        canvas.drawString("RADIO NOT FOUND", 6, kProfileY);
        drawHint(canvas);
        return;
    }

    const LoRaData& data = context.lora->data();
    canvas.setTextColor(theme::kText, theme::kBackground);
    canvas.drawString("868.0 SF12", 6, kProfileY);
    char state[24]{};
    formatState(data, state, sizeof(state));
    canvas.setTextDatum(top_right);
    canvas.setTextColor(stateColor(data.state()), theme::kBackground);
    canvas.drawString(state, config::kScreenWidth - 6, kProfileY);

    const std::size_t historySize = data.historySize();
    const std::size_t firstSlot = kLoRaHistoryCapacity - historySize;
    canvas.setTextDatum(top_left);
    for (std::size_t index = 0; index < historySize; ++index) {
        const LoRaMessageRecord& record = data.historyAt(index);
        char line[kVisibleMessageBytes + 5]{};
        std::snprintf(line, sizeof(line), "%s> %s",
                      loraMessageDirectionLabel(record.direction),
                      visibleTail(record.text.data(), record.length, kVisibleMessageBytes));
        canvas.setTextColor(record.direction == LoRaMessageDirection::Rx
                                ? theme::kPrimary
                                : theme::kSecondary,
                            theme::kBackground);
        canvas.drawString(line, 6,
                          kHistoryY + static_cast<int16_t>(firstSlot + index) * kHistoryStep);
    }

    const bool sendRejected = model_.sendRejectedVisible(context.uptimeMs);
    canvas.setTextColor(sendRejected ? theme::kWarning : theme::kMuted,
                        theme::kBackground);
    char quality[40]{};
    if (sendRejected) {
        std::snprintf(quality, sizeof(quality), "SEND REJECTED: BUSY");
    } else if (data.hasReceiveQuality()) {
        std::snprintf(quality, sizeof(quality), "RSSI %.1f dBm  SNR %.1f dB",
                      data.lastRssi(), data.lastSnr());
    } else {
        std::snprintf(quality, sizeof(quality), "RSSI --  SNR --");
    }
    canvas.drawString(quality, 6, kQualityY);

    canvas.fillRect(0, kDraftY - 2, config::kScreenWidth, 17, theme::kPanelRaised);
    canvas.setTextColor(theme::kText, theme::kPanelRaised);
    char draft[kVisibleDraftBytes + 3]{};
    std::snprintf(draft, sizeof(draft), "> %s",
                  visibleTail(data.draft(), data.draftLength(), kVisibleDraftBytes));
    canvas.drawString(draft, 6, kDraftY);
    drawHint(canvas);
}

}  // namespace pd
