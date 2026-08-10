#include "apps/remote/remote_app_model.h"

namespace pd {
namespace {

RemoteAppResult send(SonyIrCommand command) {
    return {RemoteAppEffect::SendSony, command};
}

}  // namespace

RemoteAppResult RemoteAppModel::handle(const InputEvent& event) const {
    switch (event.action) {
        case InputAction::Up: return send(SonyIrCommand::Up);
        case InputAction::Down: return send(SonyIrCommand::Down);
        case InputAction::Left: return send(SonyIrCommand::Left);
        case InputAction::Right: return send(SonyIrCommand::Right);
        case InputAction::Confirm: return send(SonyIrCommand::Ok);
        case InputAction::Erase: return send(SonyIrCommand::Back);
        case InputAction::None: break;
        default: return {};
    }

    switch (event.character) {
        case '`': return send(SonyIrCommand::Return);
        case 'p':
        case 'P': return send(SonyIrCommand::Power);
        case 'h':
        case 'H': return send(SonyIrCommand::Home);
        case 'i':
        case 'I': return send(SonyIrCommand::Input);
        case 'm':
        case 'M': return send(SonyIrCommand::Mute);
        case '-': return send(SonyIrCommand::VolumeDown);
        case '=': return send(SonyIrCommand::VolumeUp);
        default: return {};
    }
}

}  // namespace pd
