#include "apps/lora/lora_app_text.h"

namespace pd {

const char* localizedLoRaStateLabel(LoRaRadioState state,
                                    UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (state) {
            case LoRaRadioState::Unavailable: return "未发现无线电";
            case LoRaRadioState::Initializing: return "正在启动";
            case LoRaRadioState::Listening: return "正在监听";
            case LoRaRadioState::Transmitting: return "发送中";
            case LoRaRadioState::Error: return "错误";
        }
        return "未知";
    }
    switch (state) {
        case LoRaRadioState::Unavailable: return "RADIO NOT FOUND";
        case LoRaRadioState::Initializing: return "STARTING";
        case LoRaRadioState::Listening: return "LISTENING";
        case LoRaRadioState::Transmitting: return "BUSY";
        case LoRaRadioState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace pd
