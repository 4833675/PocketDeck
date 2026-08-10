#include "apps/recorder/recorder_app_text.h"

#include <cstdio>

namespace pd {

const char* localizedRecorderStateLabel(uint8_t state, UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (state) {
            case 0: return "未检测到 TF 卡";
            case 1: return "没有 WAV 文件";
            case 2: return "就绪";
            case 3: return "录音中";
            case 4: return "播放中";
            case 5: return "不支持的 WAV";
            case 6: return "损坏的 WAV";
            case 7: return "录音机错误";
        }
        return "未知";
    }
    switch (state) {
        case 0: return "NO TF CARD";
        case 1: return "NO WAV FILES";
        case 2: return "READY";
        case 3: return "RECORDING";
        case 4: return "PLAYING";
        case 5: return "UNSUPPORTED WAV";
        case 6: return "MALFORMED WAV";
        case 7: return "RECORDER ERROR";
    }
    return "UNKNOWN";
}

const char* localizedRecorderErrorLabel(uint8_t error, UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (error) {
            case 0: return "";
            case 1: return "未检测到 TF 卡";
            case 2: return "无法创建录音目录";
            case 3: return "无法打开录音目录";
            case 4: return "无法创建录音文件";
            case 5: return "没有可用文件名";
            case 6: return "WAV 头写入失败";
            case 7: return "TF 卡空间不足";
            case 8: return "麦克风启动失败";
            case 9: return "麦克风唤醒超时";
            case 10: return "麦克风队列失败";
            case 11: return "麦克风停止超时";
            case 12: return "PCM 写入失败";
            case 13: return "WAV 检查点失败";
            case 14: return "WAV 完成失败";
            case 15: return "播放文件打开失败";
            case 16: return "播放读取失败";
            case 17: return "扬声器启动失败";
            case 18: return "扬声器唤醒超时";
            case 19: return "扬声器队列失败";
            case 20: return "不支持的 WAV";
            case 21: return "损坏的 WAV";
            case 22: return "删除失败";
        }
        return "录音机错误";
    }
    switch (error) {
        case 0: return "";
        case 1: return "NO TF CARD";
        case 2: return "DIRECTORY CREATE FAILED";
        case 3: return "DIRECTORY OPEN FAILED";
        case 4: return "FILE OPEN FAILED";
        case 5: return "NO FILENAME AVAILABLE";
        case 6: return "WAV HEADER WRITE FAILED";
        case 7: return "TF CARD FULL";
        case 8: return "MIC START FAILED";
        case 9: return "MIC WAKE TIMEOUT";
        case 10: return "MIC QUEUE FAILED";
        case 11: return "MIC DRAIN TIMEOUT";
        case 12: return "PCM WRITE FAILED";
        case 13: return "WAV CHECKPOINT FAILED";
        case 14: return "WAV FINALIZE FAILED";
        case 15: return "PLAYBACK OPEN FAILED";
        case 16: return "PLAYBACK READ FAILED";
        case 17: return "SPEAKER START FAILED";
        case 18: return "SPEAKER WAKE TIMEOUT";
        case 19: return "SPEAKER QUEUE FAILED";
        case 20: return "UNSUPPORTED WAV";
        case 21: return "MALFORMED WAV";
        case 22: return "DELETE FAILED";
    }
    return "RECORDER ERROR";
}

const char* localizedRecorderLauncherTitle(UiLanguage language) {
    return localized(language, "RECORDER", "录音机");
}

const char* localizedRecorderLauncherSubtitle(UiLanguage language) {
    return localized(language, "TF card WAV recorder", "TF 卡 WAV 录音机");
}

const char* localizedRecorderHintLabel(RecorderHintContext context,
                                       UiLanguage language) {
    switch (context) {
        case RecorderHintContext::RecordIdle:
            return localized(language, "ENTER REC   TAB FILES",
                             "ENTER 录音   TAB 文件");
        case RecorderHintContext::Recording:
        case RecorderHintContext::Playing:
            return localized(language, "ENTER STOP", "ENTER 停止");
        case RecorderHintContext::FilesIdle:
            return localized(language, "ENTER PLAY   d DELETE   TAB REC",
                             "ENTER 播放   d 删除   TAB 录音");
    }
    return "";
}

bool formatRecorderElapsed(uint32_t elapsedMs, char* output,
                           std::size_t capacity) {
    if (output == nullptr || capacity == 0) return false;
    const uint32_t totalSeconds = elapsedMs / 1000u;
    const uint32_t hours = totalSeconds / 3600u;
    const uint32_t minutes = (totalSeconds / 60u) % 60u;
    const uint32_t seconds = totalSeconds % 60u;
    const int written = std::snprintf(output, capacity, "%lu:%02lu:%02lu",
                                      static_cast<unsigned long>(hours),
                                      static_cast<unsigned long>(minutes),
                                      static_cast<unsigned long>(seconds));
    return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

}  // namespace pd
