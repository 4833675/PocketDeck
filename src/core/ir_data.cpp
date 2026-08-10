#include "core/ir_data.h"

namespace pd {
namespace {

constexpr SonyIrCode kUp{151, 0x4F, 15, 2};
constexpr SonyIrCode kDown{151, 0x50, 15, 2};
constexpr SonyIrCode kLeft{151, 0x4D, 15, 2};
constexpr SonyIrCode kRight{151, 0x4E, 15, 2};
constexpr SonyIrCode kOk{151, 0x4A, 15, 2};
constexpr SonyIrCode kBack{151, 0x23, 15, 2};
constexpr SonyIrCode kReturn{1, 0x63, 12, 2};
constexpr SonyIrCode kPower{1, 0x15, 12, 2};
constexpr SonyIrCode kHome{1, 0x60, 12, 2};
constexpr SonyIrCode kInput{1, 0x25, 12, 2};
constexpr SonyIrCode kMute{1, 0x14, 12, 2};
constexpr SonyIrCode kVolumeDown{1, 0x13, 12, 2};
constexpr SonyIrCode kVolumeUp{1, 0x12, 12, 2};

}  // namespace

const SonyIrCode* sonyIrCodeFor(SonyIrCommand command) {
    switch (command) {
        case SonyIrCommand::Up: return &kUp;
        case SonyIrCommand::Down: return &kDown;
        case SonyIrCommand::Left: return &kLeft;
        case SonyIrCommand::Right: return &kRight;
        case SonyIrCommand::Ok: return &kOk;
        case SonyIrCommand::Back: return &kBack;
        case SonyIrCommand::Return: return &kReturn;
        case SonyIrCommand::Power: return &kPower;
        case SonyIrCommand::Home: return &kHome;
        case SonyIrCommand::Input: return &kInput;
        case SonyIrCommand::Mute: return &kMute;
        case SonyIrCommand::VolumeDown: return &kVolumeDown;
        case SonyIrCommand::VolumeUp: return &kVolumeUp;
        case SonyIrCommand::None: return nullptr;
    }
    return nullptr;
}

}  // namespace pd
