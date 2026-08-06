#include "apps/ssh/ssh_app_text.h"

namespace pd {

const char* localizedSshStateLabel(SshState state, UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (state) {
            case SshState::Idle: return "就绪";
            case SshState::Connecting: return "正在连接";
            case SshState::Authenticating: return "正在认证";
            case SshState::OpeningShell: return "正在打开终端";
            case SshState::Connected: return "已连接";
            case SshState::Disconnected: return "已断开";
            case SshState::Error: return "错误";
        }
        return "未知";
    }
    switch (state) {
        case SshState::Idle: return "READY";
        case SshState::Connecting: return "CONNECTING";
        case SshState::Authenticating: return "AUTHENTICATING";
        case SshState::OpeningShell: return "OPENING SHELL";
        case SshState::Connected: return "CONNECTED";
        case SshState::Disconnected: return "DISCONNECTED";
        case SshState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

const char* localizedSshErrorLabel(SshError error, UiLanguage language) {
    if (isSimplifiedChinese(language)) {
        switch (error) {
            case SshError::None: return "无";
            case SshError::NoPrivateKey: return "缺少 SSH 私钥";
            case SshError::NoNetwork: return "Wi-Fi 未连接";
            case SshError::ServiceUnavailable: return "SSH 服务不可用";
            case SshError::QueueFull: return "SSH 命令队列已满";
            case SshError::SessionCreate: return "无法创建会话";
            case SshError::Configure: return "会话配置失败";
            case SshError::Connect: return "连接失败";
            case SshError::KeyImport: return "私钥导入失败";
            case SshError::Authentication: return "公钥认证失败";
            case SshError::ChannelCreate: return "无法创建通道";
            case SshError::ChannelOpen: return "无法打开通道";
            case SshError::Pty: return "PTY 请求失败";
            case SshError::Shell: return "Shell 请求失败";
            case SshError::RemoteClosed: return "远程终端已关闭";
            case SshError::Write: return "终端写入失败";
        }
        return "未知 SSH 错误";
    }
    switch (error) {
        case SshError::None: return "None";
        case SshError::NoPrivateKey: return "SSH key missing";
        case SshError::NoNetwork: return "Wi-Fi not connected";
        case SshError::ServiceUnavailable: return "SSH service unavailable";
        case SshError::QueueFull: return "SSH command queue full";
        case SshError::SessionCreate: return "Could not create session";
        case SshError::Configure: return "Session configuration failed";
        case SshError::Connect: return "Connection failed";
        case SshError::KeyImport: return "Private key import failed";
        case SshError::Authentication: return "Public-key authentication failed";
        case SshError::ChannelCreate: return "Could not create channel";
        case SshError::ChannelOpen: return "Could not open channel";
        case SshError::Pty: return "PTY request failed";
        case SshError::Shell: return "Shell request failed";
        case SshError::RemoteClosed: return "Remote shell closed";
        case SshError::Write: return "Terminal write failed";
    }
    return "Unknown SSH error";
}

}  // namespace pd
