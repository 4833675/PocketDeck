#include "apps/media/media_app_text.h"

#include <cstring>

namespace pd {

const char* localizedMediaStateLabel(MediaPlaybackState state,
                                     UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (state) {
            case MediaPlaybackState::NoCard: return "未检测到 TF 卡";
            case MediaPlaybackState::Empty: return "没有 MP3 文件";
            case MediaPlaybackState::Ready: return "就绪";
            case MediaPlaybackState::Playing: return "播放中";
            case MediaPlaybackState::Paused: return "已暂停";
            case MediaPlaybackState::Error: return "媒体错误";
        }
        return "未知";
    }
    switch (state) {
        case MediaPlaybackState::NoCard: return "NO TF CARD";
        case MediaPlaybackState::Empty: return "NO MP3 FILES";
        case MediaPlaybackState::Ready: return "READY";
        case MediaPlaybackState::Playing: return "PLAYING";
        case MediaPlaybackState::Paused: return "PAUSED";
        case MediaPlaybackState::Error: return "MEDIA ERROR";
    }
    return "UNKNOWN";
}

const char* localizedMediaDetailLabel(const char* detail,
                                      UiLanguage language) {
    if (detail == nullptr) return "";
    if (!isSimplifiedChinese(language)) return detail;
    if (std::strcmp(detail, "MOUNT TF IN SETTINGS") == 0) {
        return "请在设置中挂载 TF 卡";
    }
    if (std::strcmp(detail, "COULD NOT CREATE /Music") == 0) {
        return "无法创建 /Music";
    }
    if (std::strcmp(detail, "COULD NOT OPEN FOLDER") == 0) return "无法打开文件夹";
    if (std::strcmp(detail, "COPY MP3 TO /Music") == 0) return "请将 MP3 放入 /Music";
    if (std::strcmp(detail, "EMPTY FOLDER") == 0) return "文件夹为空";
    if (std::strcmp(detail, "SHOWING FIRST 64") == 0) return "仅显示前 64 项";
    if (std::strcmp(detail, "MP3 DECODE FAILED") == 0) return "MP3 解码失败";
    if (std::strcmp(detail, "NEXT TRACK FAILED") == 0) return "下一首播放失败";
    if (std::strcmp(detail, "OUT OF MEMORY") == 0) return "内存不足";
    if (std::strcmp(detail, "COULD NOT OPEN MP3") == 0) return "无法打开 MP3";
    if (std::strcmp(detail, "MP3 START FAILED") == 0) return "MP3 启动失败";
    if (std::strcmp(detail, "SERVICE UNAVAILABLE") == 0) return "媒体服务不可用";
    return detail;
}

}  // namespace pd
