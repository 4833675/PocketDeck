#include "test_harness.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "pocket_deck_config.h"
#include "apps/launcher/launcher_model.h"
#include "apps/gps/gps_app_model.h"
#include "apps/gps/gps_app_text.h"
#include "apps/keyboard/keyboard_app_text.h"
#include "apps/lora/lora_app_model.h"
#include "apps/lora/lora_app_text.h"
#include "apps/media/media_app_model.h"
#include "apps/media/media_app_text.h"
#include "apps/settings/settings_model.h"
#include "apps/settings/settings_app_text.h"
#include "apps/weather/weather_app_text.h"
#include "apps/ssh/ssh_app_model.h"
#include "apps/ssh/ssh_app_text.h"
#include "core/ble_keyboard_policy.h"
#include "core/clock_data.h"
#include "core/g0_gesture.h"
#include "core/gps_data.h"
#include "core/lora_data.h"
#include "core/lora_tx_policy.h"
#include "core/media_data.h"
#include "core/motion_data.h"
#include "core/input_router.h"
#include "core/mac_keymap.h"
#include "core/resource_policy.h"
#include "core/serial_command.h"
#include "core/ssh_error_detail.h"
#include "core/ssh_hosts.h"
#include "core/ssh_host_record.h"
#include "core/ssh_memory_budget.h"
#include "core/ssh_retry_policy.h"
#include "core/ssh_transport_profile.h"
#include "core/system_settings.h"
#include "core/system_context.h"
#include "core/terminal_buffer.h"
#include "core/terminal_input.h"
#include "core/text_keymap.h"
#include "core/weather_data.h"
#include "core/wifi_data.h"
#include "core/wifi_profiles.h"
#include "core/wifi_recovery_policy.h"
#include "services/diagnostics_service.h"
#include "ui/quick_settings_model.h"
#include "ui/theme.h"

using namespace pd;

namespace {

struct DiagnosticSinkCapture {
    std::array<std::array<char, DiagnosticsService::kAsyncMessageCapacity>, 8> messages{};
    std::size_t count = 0;
};

void captureDiagnostic(void* context, const char* message) {
    auto* capture = static_cast<DiagnosticSinkCapture*>(context);
    if (capture == nullptr || capture->count >= capture->messages.size()) return;
    std::strncpy(capture->messages[capture->count].data(), message,
                 capture->messages[capture->count].size() - 1);
    ++capture->count;
}

}  // namespace

TEST_CASE(app_resource_profiles_isolate_foreground_workloads) {
    const auto launcher = resourceProfileFor(AppId::Launcher);
    CHECK(!launcher.needs(RuntimeResource::Ble));
    CHECK(!launcher.needs(RuntimeResource::Wifi));
    CHECK(!launcher.needs(RuntimeResource::Gps));
    CHECK(!launcher.needs(RuntimeResource::LoRa));

    const auto keyboard = resourceProfileFor(AppId::Keyboard);
    CHECK(keyboard.needs(RuntimeResource::Ble));
    CHECK(!keyboard.needs(RuntimeResource::Wifi));

    const auto ssh = resourceProfileFor(AppId::Ssh);
    CHECK(ssh.needs(RuntimeResource::Wifi));
    CHECK(!ssh.needs(RuntimeResource::Ble));

    const auto gps = resourceProfileFor(AppId::Gps);
    CHECK(gps.needs(RuntimeResource::Gps));
    CHECK(!gps.needs(RuntimeResource::Wifi));

    const auto lora = resourceProfileFor(AppId::LoRa);
    CHECK(lora.needs(RuntimeResource::LoRa));
    CHECK(!lora.needs(RuntimeResource::Gps));
}

TEST_CASE(weather_and_media_profiles_have_explicit_tradeoffs) {
    const auto weather = resourceProfileFor(AppId::Weather);
    CHECK(weather.needs(RuntimeResource::Wifi));
    CHECK(weather.needs(RuntimeResource::Gps));
    CHECK(!weather.needs(RuntimeResource::Ble));
    CHECK(!weather.needs(RuntimeResource::LoRa));

    const auto media = resourceProfileFor(AppId::Media);
    CHECK(media.needs(RuntimeResource::MediaRealtime));
    CHECK(!media.needs(RuntimeResource::Wifi));
    CHECK(!media.needs(RuntimeResource::Ble));
    CHECK(!media.needs(RuntimeResource::Gps));
    CHECK(!media.needs(RuntimeResource::LoRa));

    const auto settings = resourceProfileFor(AppId::Settings);
    CHECK(settings.needs(RuntimeResource::Wifi));
    CHECK(settings.needs(RuntimeResource::Ble));
}

TEST_CASE(ssh_error_detail_redacts_endpoint_identity) {
    std::array<char, 96> output{};
    sanitizeSshErrorDetail("Timeout connecting to deck.example as kexin",
                           "deck.example", "kexin", output.data(), output.size());
    CHECK_STR_EQ(output.data(), "Timeout connecting to <host> as <user>");
}

TEST_CASE(ssh_error_detail_stays_single_line_and_never_partially_copies_secret) {
    std::array<char, 24> output{};
    sanitizeSshErrorDetail("failure\nprivate-hostname", "private-hostname", "",
                           output.data(), output.size());
    CHECK_STR_EQ(output.data(), "failure <host>");
    CHECK(std::strstr(output.data(), "private") == nullptr);

    std::array<char, 10> shortOutput{};
    sanitizeSshErrorDetail("x private-hostname", "private-hostname", "",
                           shortOutput.data(), shortOutput.size());
    CHECK(std::strstr(shortOutput.data(), "private") == nullptr);
}

TEST_CASE(ssh_retry_waits_after_failure_not_after_attempt_start) {
    SshRetryPolicy policy;
    policy.noteFailure(8000);
    CHECK_EQ(policy.secondsRemaining(8000), 5u);
    CHECK(!policy.takeDue(12999));
    CHECK(policy.takeDue(13000));
    CHECK(!policy.takeDue(13001));
}

TEST_CASE(ssh_retry_is_cancelable_and_wrap_safe) {
    SshRetryPolicy policy;
    policy.noteFailure(1000);
    policy.cancel();
    CHECK(!policy.takeDue(100000));

    policy.noteFailure(0xFFFFFF00u);
    CHECK(!policy.takeDue(0x00001287u));
    CHECK(policy.takeDue(0x00001288u));
}

TEST_CASE(ssh_transport_uses_the_low_memory_cipher_in_both_directions) {
    CHECK_STR_EQ(ssh_transport::kCipher, "aes128-ctr");
}

TEST_CASE(plain_a) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::A}),
             HidReport::single(0x00, 0x04));
}

TEST_CASE(mac_modifiers) {
    const KeyState keys{PhysicalKey::Ctrl, PhysicalKey::Opt, PhysicalKey::Alt,
                        PhysicalKey::Shift, PhysicalKey::A};
    CHECK_EQ(MacKeymap::buildReport(keys), HidReport::single(0x0F, 0x04));
}

TEST_CASE(fn_navigation_and_escape) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Semicolon}),
             HidReport::single(0x00, 0x52));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Comma}),
             HidReport::single(0x00, 0x50));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Period}),
             HidReport::single(0x00, 0x51));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Slash}),
             HidReport::single(0x00, 0x4F));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Backtick}),
             HidReport::single(0x00, 0x29));
}

TEST_CASE(fn_function_and_editing_keys) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Num1}),
             HidReport::single(0x00, 0x3A));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Equal}),
             HidReport::single(0x00, 0x45));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Backspace}),
             HidReport::single(0x00, 0x4C));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Tab}),
             HidReport::single(0x00, 0x39));
    CHECK(MacKeymap::buildReport(KeyState{PhysicalKey::Fn}).empty());
}

TEST_CASE(punctuation_remains_available_without_fn) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Semicolon}),
             HidReport::single(0x00, 0x33));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Backtick}),
             HidReport::single(0x00, 0x35));
}

TEST_CASE(all_printable_keys_use_us_hid_usages) {
    constexpr std::array<std::pair<PhysicalKey, uint8_t>, 47> expected{{
        {PhysicalKey::Backtick, 0x35}, {PhysicalKey::Num1, 0x1E},
        {PhysicalKey::Num2, 0x1F}, {PhysicalKey::Num3, 0x20},
        {PhysicalKey::Num4, 0x21}, {PhysicalKey::Num5, 0x22},
        {PhysicalKey::Num6, 0x23}, {PhysicalKey::Num7, 0x24},
        {PhysicalKey::Num8, 0x25}, {PhysicalKey::Num9, 0x26},
        {PhysicalKey::Num0, 0x27}, {PhysicalKey::Minus, 0x2D},
        {PhysicalKey::Equal, 0x2E}, {PhysicalKey::Backspace, 0x2A},
        {PhysicalKey::Tab, 0x2B}, {PhysicalKey::Q, 0x14},
        {PhysicalKey::W, 0x1A}, {PhysicalKey::E, 0x08},
        {PhysicalKey::R, 0x15}, {PhysicalKey::T, 0x17},
        {PhysicalKey::Y, 0x1C}, {PhysicalKey::U, 0x18},
        {PhysicalKey::I, 0x0C}, {PhysicalKey::O, 0x12},
        {PhysicalKey::P, 0x13}, {PhysicalKey::LeftBracket, 0x2F},
        {PhysicalKey::RightBracket, 0x30}, {PhysicalKey::Backslash, 0x31},
        {PhysicalKey::A, 0x04}, {PhysicalKey::S, 0x16},
        {PhysicalKey::D, 0x07}, {PhysicalKey::F, 0x09},
        {PhysicalKey::G, 0x0A}, {PhysicalKey::H, 0x0B},
        {PhysicalKey::J, 0x0D}, {PhysicalKey::K, 0x0E},
        {PhysicalKey::L, 0x0F}, {PhysicalKey::Semicolon, 0x33},
        {PhysicalKey::Apostrophe, 0x34}, {PhysicalKey::Enter, 0x28},
        {PhysicalKey::Z, 0x1D}, {PhysicalKey::X, 0x1B},
        {PhysicalKey::C, 0x06}, {PhysicalKey::V, 0x19},
        {PhysicalKey::B, 0x05}, {PhysicalKey::N, 0x11},
        {PhysicalKey::M, 0x10},
    }};

    for (const auto& [key, usage] : expected) {
        CHECK_EQ(MacKeymap::buildReport(KeyState{key}), HidReport::single(0, usage));
    }

    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Comma}),
             HidReport::single(0, 0x36));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Period}),
             HidReport::single(0, 0x37));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Slash}),
             HidReport::single(0, 0x38));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Space}),
             HidReport::single(0, 0x2C));
}

TEST_CASE(more_than_six_keys_reports_rollover) {
    const KeyState keys{PhysicalKey::A, PhysicalKey::B, PhysicalKey::C,
                        PhysicalKey::D, PhysicalKey::E, PhysicalKey::F,
                        PhysicalKey::G};
    const auto report = MacKeymap::buildReport(keys);
    for (const uint8_t usage : report.keys) CHECK_EQ(usage, 0x01);
}

TEST_CASE(system_fn_navigation_is_local_and_edge_triggered) {
    InputRouter router;
    const KeyState left{PhysicalKey::Fn, PhysicalKey::Comma};
    auto frame = router.update(left, InputMode::System, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Left);
    CHECK(!frame.hasHidReport);

    frame = router.update(left, InputMode::System, false);
    CHECK_EQ(frame.eventCount, 0);
}

TEST_CASE(system_confirm_back_and_tab_are_local) {
    InputRouter router;
    auto frame = router.update(
        KeyState{PhysicalKey::Enter, PhysicalKey::Backspace, PhysicalKey::Tab},
        InputMode::System, false);
    CHECK_EQ(frame.eventCount, 3);
    CHECK_EQ(frame.events[0].action, InputAction::Confirm);
    CHECK_EQ(frame.events[1].action, InputAction::Back);
    CHECK_EQ(frame.events[2].action, InputAction::Tab);
}

TEST_CASE(text_keymap_covers_wifi_password_characters) {
    CHECK_EQ(TextKeymap::character(PhysicalKey::A, false), 'a');
    CHECK_EQ(TextKeymap::character(PhysicalKey::A, true), 'A');
    CHECK_EQ(TextKeymap::character(PhysicalKey::Num2, true), '@');
    CHECK_EQ(TextKeymap::character(PhysicalKey::Slash, true), '?');
    CHECK_EQ(TextKeymap::character(PhysicalKey::Space, false), ' ');
    CHECK_EQ(TextKeymap::character(PhysicalKey::Shift, false), '\0');
}

TEST_CASE(text_mode_is_local_and_uses_explicit_edit_actions) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Text, true);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].character, 'a');
    CHECK(!frame.hasHidReport);

    frame = router.update(KeyState{}, InputMode::Text, true);
    frame = router.update(KeyState{PhysicalKey::Backspace}, InputMode::Text, true);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Erase);

    frame = router.update(KeyState{}, InputMode::Text, true);
    frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Backtick}, InputMode::Text,
                          true);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Back);
}

TEST_CASE(text_mode_uses_fn_navigation_to_move_between_fields) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Semicolon},
                               InputMode::Text, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Up);

    router.reset();
    frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Period},
                          InputMode::Text, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Down);
}

TEST_CASE(text_mode_exposes_tab_for_multi_field_editors) {
    InputRouter router;
    const auto frame = router.update(KeyState{PhysicalKey::Tab}, InputMode::Text, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Tab);
}

TEST_CASE(terminal_mode_emits_local_printable_characters) {
    InputRouter router;
    const auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Terminal, true);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].character, 'a');
    CHECK(!frame.hasHidReport);
}

TEST_CASE(terminal_mode_converts_ctrl_letters_to_control_bytes) {
    InputRouter router;
    const auto frame = router.update(
        KeyState{PhysicalKey::Ctrl, PhysicalKey::C}, InputMode::Terminal, true);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(static_cast<unsigned char>(frame.events[0].character), 0x03u);
}

TEST_CASE(terminal_mode_exposes_navigation_and_terminal_controls) {
    InputRouter router;
    auto frame = router.update(
        KeyState{PhysicalKey::Fn, PhysicalKey::Semicolon}, InputMode::Terminal, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Up);

    router.reset();
    frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Backtick},
                          InputMode::Terminal, false);
    CHECK_EQ(frame.events[0].action, InputAction::Escape);

    router.reset();
    frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Backspace},
                          InputMode::Terminal, false);
    CHECK_EQ(frame.events[0].action, InputAction::DeleteForward);

    router.reset();
    frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Tab},
                          InputMode::Terminal, false);
    CHECK_EQ(frame.events[0].action, InputAction::QuickCommands);
}

TEST_CASE(terminal_mode_reserves_option_navigation_for_local_scrollback) {
    InputRouter router;
    auto frame = router.update(
        KeyState{PhysicalKey::Opt, PhysicalKey::Fn, PhysicalKey::Semicolon},
        InputMode::Terminal, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::ScrollUp);

    router.reset();
    frame = router.update(
        KeyState{PhysicalKey::Opt, PhysicalKey::Fn, PhysicalKey::Period},
        InputMode::Terminal, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::ScrollDown);
}

TEST_CASE(terminal_input_encodes_shell_control_sequences) {
    CHECK_EQ(encodeTerminalInput({InputAction::None, 'x'}), TerminalInput::from("x"));
    CHECK_EQ(encodeTerminalInput({InputAction::Confirm, '\0'}), TerminalInput::from("\r"));
    CHECK_EQ(encodeTerminalInput({InputAction::Erase, '\0'}), TerminalInput::from("\x7f"));
    CHECK_EQ(encodeTerminalInput({InputAction::Tab, '\0'}), TerminalInput::from("\t"));
    CHECK_EQ(encodeTerminalInput({InputAction::Up, '\0'}), TerminalInput::from("\x1b[A"));
    CHECK_EQ(encodeTerminalInput({InputAction::Down, '\0'}), TerminalInput::from("\x1b[B"));
    CHECK_EQ(encodeTerminalInput({InputAction::Right, '\0'}), TerminalInput::from("\x1b[C"));
    CHECK_EQ(encodeTerminalInput({InputAction::Left, '\0'}), TerminalInput::from("\x1b[D"));
    CHECK_EQ(encodeTerminalInput({InputAction::Escape, '\0'}), TerminalInput::from("\x1b"));
    CHECK_EQ(encodeTerminalInput({InputAction::DeleteForward, '\0'}),
             TerminalInput::from("\x1b[3~"));
    CHECK(encodeTerminalInput({InputAction::QuickCommands, '\0'}).empty());
}

TEST_CASE(keyboard_input_is_dropped_when_disconnected) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, false);
    CHECK_EQ(frame.eventCount, 0);
    CHECK(!frame.hasHidReport);
}

TEST_CASE(keyboard_input_becomes_hid_when_connected) {
    InputRouter router;
    router.update(KeyState{}, InputMode::Keyboard, true);
    const auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, true);
    CHECK(frame.hasHidReport);
    CHECK_EQ(frame.hidReport, HidReport::single(0, 0x04));
}

TEST_CASE(enter_used_to_open_keyboard_does_not_leak_to_mac) {
    InputRouter router;
    router.update(KeyState{PhysicalKey::Enter}, InputMode::System, false);
    auto frame = router.update(KeyState{PhysicalKey::Enter}, InputMode::Keyboard, true);
    CHECK(!frame.hasHidReport);

    frame = router.update(KeyState{}, InputMode::Keyboard, true);
    CHECK(frame.hasHidReport);
    CHECK(frame.hidReport.empty());
}

TEST_CASE(reconnect_waits_for_held_keys_to_release) {
    InputRouter router;
    router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, false);
    auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, true);
    CHECK(!frame.hasHidReport);
    frame = router.update(KeyState{}, InputMode::Keyboard, true);
    CHECK(frame.hasHidReport);
    CHECK(frame.hidReport.empty());
}

TEST_CASE(g0_long_suppresses_short) {
    G0Gesture gesture(600, 25);
    CHECK_EQ(gesture.update(true, 100), G0Action::None);
    CHECK_EQ(gesture.update(true, 130), G0Action::None);
    CHECK_EQ(gesture.update(true, 730), G0Action::QuickSettings);
    CHECK_EQ(gesture.update(false, 740), G0Action::None);
    CHECK_EQ(gesture.update(false, 770), G0Action::None);
}

TEST_CASE(g0_short_fires_on_release) {
    G0Gesture gesture(600, 25);
    CHECK_EQ(gesture.update(true, 100), G0Action::None);
    CHECK_EQ(gesture.update(true, 130), G0Action::None);
    CHECK_EQ(gesture.update(false, 250), G0Action::None);
    CHECK_EQ(gesture.update(false, 280), G0Action::Home);
}

TEST_CASE(settings_defaults_are_valid) {
    const auto settings = SystemSettings::defaults();
    CHECK_EQ(settings.version, SystemSettings::kVersion);
    CHECK_EQ(settings.brightness, 78);
    CHECK_EQ(settings.volume, 55);
    CHECK_EQ(settings.sleepSeconds, 120);
    CHECK(settings.keyClick);
    CHECK(!settings.wifiEnabled);
    CHECK(settings.bleEnabled);
    CHECK_EQ(settings.language, UiLanguage::English);
    CHECK_STR_EQ(settings.deviceName.data(), "Pocket Deck");
    CHECK_STR_EQ(settings.hostLabel.data(), "Mac");
}

TEST_CASE(settings_version_mismatch_returns_defaults) {
    auto settings = SystemSettings::defaults();
    settings.version = 99;
    settings.brightness = 255;
    const auto fixed = sanitizeSettings(settings);
    CHECK_EQ(fixed.version, SystemSettings::kVersion);
    CHECK_EQ(fixed.brightness, 78);
}

TEST_CASE(settings_sanitizer_clamps_and_repairs) {
    auto settings = SystemSettings::defaults();
    settings.brightness = 255;
    settings.volume = 200;
    settings.sleepSeconds = 1;
    settings.language = static_cast<UiLanguage>(99);
    settings.deviceName.fill('\0');
    settings.hostLabel.fill('X');
    const auto fixed = sanitizeSettings(settings);
    CHECK_EQ(fixed.brightness, 100);
    CHECK_EQ(fixed.volume, 100);
    CHECK_EQ(fixed.sleepSeconds, 15);
    CHECK_EQ(fixed.language, UiLanguage::English);
    CHECK_STR_EQ(fixed.deviceName.data(), "Pocket Deck");
    CHECK_STR_EQ(fixed.hostLabel.data(), "Mac");
}

TEST_CASE(localization_switches_without_dynamic_storage) {
    CHECK_STR_EQ(localized(UiLanguage::English, "Settings", "设置"), "Settings");
    CHECK_STR_EQ(localized(UiLanguage::SimplifiedChinese, "Settings", "设置"), "设置");
    CHECK_EQ(toggledUiLanguage(UiLanguage::English), UiLanguage::SimplifiedChinese);
    CHECK_EQ(toggledUiLanguage(UiLanguage::SimplifiedChinese), UiLanguage::English);
}

TEST_CASE(launcher_starts_on_keyboard_and_wraps) {
    LauncherModel model;
    CHECK_EQ(model.selected(), AppId::Keyboard);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Ssh);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Gps);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::LoRa);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Media);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Weather);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Settings);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Keyboard);
    model.handle(InputAction::Left);
    CHECK_EQ(model.selected(), AppId::Settings);
}

TEST_CASE(launcher_confirm_requests_selected_app) {
    LauncherModel model;
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Keyboard);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Ssh);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Gps);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::LoRa);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Media);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Weather);
    model.handle(InputAction::Right);
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Settings);
    CHECK_EQ(model.handle(InputAction::Back), AppId::None);
}

TEST_CASE(wifi_and_weather_labels_cover_visible_states) {
    CHECK_STR_EQ(wifiStateLabel(WifiState::Disabled), "OFF");
    CHECK_STR_EQ(wifiStateLabel(WifiState::Scanning), "SCANNING");
    CHECK_STR_EQ(wifiStateLabel(WifiState::Connected), "CONNECTED");
    CHECK_STR_EQ(wifiDisconnectReasonLabel(8), "ASSOC_LEAVE");
    CHECK_STR_EQ(wifiDisconnectReasonLabel(200), "BEACON_TIMEOUT");
    CHECK_STR_EQ(wifiDisconnectReasonLabel(201), "NO_AP_FOUND");
    CHECK_STR_EQ(wifiDisconnectReasonLabel(255), "OTHER");
    CHECK_STR_EQ(weatherStateLabel(WeatherState::Fetching), "FETCHING");
    CHECK_STR_EQ(weatherCodeLabel(0), "CLEAR");
    CHECK_STR_EQ(weatherCodeLabel(61), "RAIN");
    CHECK_STR_EQ(weatherCodeLabel(95), "THUNDERSTORM");
}

TEST_CASE(weather_localized_text_covers_conditions_states_and_errors) {
    CHECK_STR_EQ(localizedWeatherCodeLabel(0, UiLanguage::English), "CLEAR");
    CHECK_STR_EQ(localizedWeatherCodeLabel(0, UiLanguage::SimplifiedChinese), "晴");
    CHECK_STR_EQ(localizedWeatherCodeLabel(1, UiLanguage::SimplifiedChinese),
                 "大部晴朗");
    CHECK_STR_EQ(localizedWeatherCodeLabel(2, UiLanguage::SimplifiedChinese),
                 "局部多云");
    CHECK_STR_EQ(localizedWeatherCodeLabel(45, UiLanguage::SimplifiedChinese), "雾");
    CHECK_STR_EQ(localizedWeatherCodeLabel(51, UiLanguage::SimplifiedChinese),
                 "毛毛雨");
    CHECK_STR_EQ(localizedWeatherCodeLabel(61, UiLanguage::SimplifiedChinese), "雨");
    CHECK_STR_EQ(localizedWeatherCodeLabel(71, UiLanguage::SimplifiedChinese), "雪");
    CHECK_STR_EQ(localizedWeatherCodeLabel(80, UiLanguage::SimplifiedChinese),
                 "阵雨");
    CHECK_STR_EQ(localizedWeatherCodeLabel(85, UiLanguage::SimplifiedChinese),
                 "阵雪");
    CHECK_STR_EQ(localizedWeatherCodeLabel(95, UiLanguage::SimplifiedChinese),
                 "雷暴");
    CHECK_STR_EQ(localizedWeatherCodeLabel(49, UiLanguage::SimplifiedChinese),
                 "未知");

    CHECK_STR_EQ(localizedWeatherDisplayLabel(WeatherDisplayState::Live,
                                              UiLanguage::SimplifiedChinese),
                 "实时");
    CHECK_STR_EQ(localizedWeatherDisplayLabel(WeatherDisplayState::CachedNoGps,
                                              UiLanguage::SimplifiedChinese),
                 "缓存");
    CHECK_STR_EQ(localizedWeatherDisplayLabel(WeatherDisplayState::WifiOff,
                                              UiLanguage::SimplifiedChinese),
                 "Wi-Fi 已关闭");
    CHECK_STR_EQ(localizedWeatherDisplayLabel(WeatherDisplayState::WaitingGps,
                                              UiLanguage::SimplifiedChinese),
                 "等待 GPS 定位");
    CHECK_STR_EQ(localizedWeatherDisplayLabel(WeatherDisplayState::Error,
                                              UiLanguage::English),
                 "WEATHER ERROR");
    CHECK_STR_EQ(localizedWeatherWifiStateLabel(WifiState::Scanning,
                                                UiLanguage::SimplifiedChinese),
                 "正在扫描");
    CHECK_STR_EQ(localizedWeatherWifiStateLabel(WifiState::Connecting,
                                                UiLanguage::SimplifiedChinese),
                 "正在连接");
    CHECK_STR_EQ(localizedWeatherErrorLabel("Weather HTTP request failed",
                                            UiLanguage::SimplifiedChinese),
                 "天气请求失败");
    CHECK_STR_EQ(localizedWeatherErrorLabel("custom detail",
                                            UiLanguage::SimplifiedChinese),
                 "custom detail");
}

TEST_CASE(keyboard_localized_text_covers_every_state_and_error) {
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Disabled,
                                             UiLanguage::English),
                 "BLUETOOTH OFF");
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Disabled,
                                             UiLanguage::SimplifiedChinese),
                 "蓝牙已关闭");
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Advertising,
                                             UiLanguage::SimplifiedChinese),
                 "等待 Mac");
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Pairing,
                                             UiLanguage::SimplifiedChinese),
                 "正在配对");
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Connected,
                                             UiLanguage::SimplifiedChinese),
                 "已连接");
    CHECK_STR_EQ(localizedKeyboardStateLabel(BleKeyboardState::Error,
                                             UiLanguage::SimplifiedChinese),
                 "错误");

    CHECK_STR_EQ(localizedKeyboardErrorLabel(BleKeyboardError::None,
                                             UiLanguage::SimplifiedChinese),
                 "无");
    CHECK_STR_EQ(localizedKeyboardErrorLabel(BleKeyboardError::InitializationFailed,
                                             UiLanguage::SimplifiedChinese),
                 "初始化失败");
    CHECK_STR_EQ(localizedKeyboardErrorLabel(BleKeyboardError::UnauthorizedPeer,
                                             UiLanguage::SimplifiedChinese),
                 "已拒绝未知主机");
    CHECK_STR_EQ(localizedKeyboardErrorLabel(BleKeyboardError::AuthenticationFailed,
                                             UiLanguage::SimplifiedChinese),
                 "认证失败");
    CHECK_STR_EQ(localizedKeyboardErrorLabel(BleKeyboardError::BondOperationFailed,
                                             UiLanguage::SimplifiedChinese),
                 "配对记录操作失败");
}

TEST_CASE(ssh_localized_text_covers_every_state_and_error) {
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Idle, UiLanguage::English), "READY");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Idle, UiLanguage::SimplifiedChinese),
                 "就绪");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Connecting,
                                       UiLanguage::SimplifiedChinese),
                 "正在连接");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Authenticating,
                                       UiLanguage::SimplifiedChinese),
                 "正在认证");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::OpeningShell,
                                       UiLanguage::SimplifiedChinese),
                 "正在打开终端");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Connected,
                                       UiLanguage::SimplifiedChinese),
                 "已连接");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Disconnected,
                                       UiLanguage::SimplifiedChinese),
                 "已断开");
    CHECK_STR_EQ(localizedSshStateLabel(SshState::Error,
                                       UiLanguage::SimplifiedChinese),
                 "错误");

    CHECK_STR_EQ(localizedSshErrorLabel(SshError::None, UiLanguage::SimplifiedChinese),
                 "无");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::NoPrivateKey,
                                       UiLanguage::SimplifiedChinese),
                 "缺少 SSH 私钥");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::NoNetwork,
                                       UiLanguage::SimplifiedChinese),
                 "Wi-Fi 未连接");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::ServiceUnavailable,
                                       UiLanguage::SimplifiedChinese),
                 "SSH 服务不可用");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::QueueFull,
                                       UiLanguage::SimplifiedChinese),
                 "SSH 命令队列已满");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::SessionCreate,
                                       UiLanguage::SimplifiedChinese),
                 "无法创建会话");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Configure,
                                       UiLanguage::SimplifiedChinese),
                 "会话配置失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Connect,
                                       UiLanguage::SimplifiedChinese),
                 "连接失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::KeyImport,
                                       UiLanguage::SimplifiedChinese),
                 "私钥导入失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Authentication,
                                       UiLanguage::SimplifiedChinese),
                 "公钥认证失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::ChannelCreate,
                                       UiLanguage::SimplifiedChinese),
                 "无法创建通道");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::ChannelOpen,
                                       UiLanguage::SimplifiedChinese),
                 "无法打开通道");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Pty,
                                       UiLanguage::SimplifiedChinese),
                 "PTY 请求失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Shell,
                                       UiLanguage::SimplifiedChinese),
                 "Shell 请求失败");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::RemoteClosed,
                                       UiLanguage::SimplifiedChinese),
                 "远程终端已关闭");
    CHECK_STR_EQ(localizedSshErrorLabel(SshError::Write,
                                       UiLanguage::SimplifiedChinese),
                 "终端写入失败");
}

TEST_CASE(lora_localized_text_covers_every_radio_state) {
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Unavailable,
                                        UiLanguage::English),
                 "RADIO NOT FOUND");
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Unavailable,
                                        UiLanguage::SimplifiedChinese),
                 "未发现无线电");
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Initializing,
                                        UiLanguage::SimplifiedChinese),
                 "正在启动");
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Listening,
                                        UiLanguage::SimplifiedChinese),
                 "正在监听");
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Transmitting,
                                        UiLanguage::SimplifiedChinese),
                 "发送中");
    CHECK_STR_EQ(localizedLoRaStateLabel(LoRaRadioState::Error,
                                        UiLanguage::SimplifiedChinese),
                 "错误");
}

TEST_CASE(media_localized_text_covers_states_known_details_and_passthrough) {
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::NoCard,
                                         UiLanguage::English),
                 "NO TF CARD");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::NoCard,
                                         UiLanguage::SimplifiedChinese),
                 "未检测到 TF 卡");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::Empty,
                                         UiLanguage::SimplifiedChinese),
                 "没有 MP3 文件");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::Ready,
                                         UiLanguage::SimplifiedChinese),
                 "就绪");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::Playing,
                                         UiLanguage::SimplifiedChinese),
                 "播放中");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::Paused,
                                         UiLanguage::SimplifiedChinese),
                 "已暂停");
    CHECK_STR_EQ(localizedMediaStateLabel(MediaPlaybackState::Error,
                                         UiLanguage::SimplifiedChinese),
                 "媒体错误");

    CHECK_STR_EQ(localizedMediaDetailLabel("MOUNT TF IN SETTINGS",
                                          UiLanguage::SimplifiedChinese),
                 "请在设置中挂载 TF 卡");
    CHECK_STR_EQ(localizedMediaDetailLabel("COULD NOT CREATE /Music",
                                          UiLanguage::SimplifiedChinese),
                 "无法创建 /Music");
    CHECK_STR_EQ(localizedMediaDetailLabel("COULD NOT OPEN FOLDER",
                                          UiLanguage::SimplifiedChinese),
                 "无法打开文件夹");
    CHECK_STR_EQ(localizedMediaDetailLabel("COPY MP3 TO /Music",
                                          UiLanguage::SimplifiedChinese),
                 "请将 MP3 放入 /Music");
    CHECK_STR_EQ(localizedMediaDetailLabel("EMPTY FOLDER",
                                          UiLanguage::SimplifiedChinese),
                 "文件夹为空");
    CHECK_STR_EQ(localizedMediaDetailLabel("SHOWING FIRST 64",
                                          UiLanguage::SimplifiedChinese),
                 "仅显示前 64 项");
    CHECK_STR_EQ(localizedMediaDetailLabel("MP3 DECODE FAILED",
                                          UiLanguage::SimplifiedChinese),
                 "MP3 解码失败");
    CHECK_STR_EQ(localizedMediaDetailLabel("NEXT TRACK FAILED",
                                          UiLanguage::SimplifiedChinese),
                 "下一首播放失败");
    CHECK_STR_EQ(localizedMediaDetailLabel("OUT OF MEMORY",
                                          UiLanguage::SimplifiedChinese),
                 "内存不足");
    CHECK_STR_EQ(localizedMediaDetailLabel("COULD NOT OPEN MP3",
                                          UiLanguage::SimplifiedChinese),
                 "无法打开 MP3");
    CHECK_STR_EQ(localizedMediaDetailLabel("MP3 START FAILED",
                                          UiLanguage::SimplifiedChinese),
                 "MP3 启动失败");
    CHECK_STR_EQ(localizedMediaDetailLabel("SERVICE UNAVAILABLE",
                                          UiLanguage::SimplifiedChinese),
                 "媒体服务不可用");
    CHECK_STR_EQ(localizedMediaDetailLabel("future detail",
                                          UiLanguage::SimplifiedChinese),
                 "future detail");
}

TEST_CASE(settings_localized_text_covers_reset_and_storage_failures) {
    CHECK_STR_EQ(localizedResetReasonLabel("power-on", UiLanguage::English), "power-on");
    CHECK_STR_EQ(localizedResetReasonLabel("power-on", UiLanguage::SimplifiedChinese),
                 "上电");
    CHECK_STR_EQ(localizedResetReasonLabel("external", UiLanguage::SimplifiedChinese),
                 "外部复位");
    CHECK_STR_EQ(localizedResetReasonLabel("software", UiLanguage::SimplifiedChinese),
                 "软件重启");
    CHECK_STR_EQ(localizedResetReasonLabel("panic", UiLanguage::SimplifiedChinese),
                 "崩溃");
    CHECK_STR_EQ(localizedResetReasonLabel("interrupt-wdt", UiLanguage::SimplifiedChinese),
                 "中断看门狗");
    CHECK_STR_EQ(localizedResetReasonLabel("task-wdt", UiLanguage::SimplifiedChinese),
                 "任务看门狗");
    CHECK_STR_EQ(localizedResetReasonLabel("watchdog", UiLanguage::SimplifiedChinese),
                 "看门狗");
    CHECK_STR_EQ(localizedResetReasonLabel("deep-sleep", UiLanguage::SimplifiedChinese),
                 "深度睡眠唤醒");
    CHECK_STR_EQ(localizedResetReasonLabel("brownout", UiLanguage::SimplifiedChinese),
                 "电压过低");
    CHECK_STR_EQ(localizedResetReasonLabel("sdio", UiLanguage::SimplifiedChinese), "SDIO");
    CHECK_STR_EQ(localizedResetReasonLabel("unknown", UiLanguage::SimplifiedChinese),
                 "未知");
    CHECK_STR_EQ(localizedResetReasonLabel("other", UiLanguage::SimplifiedChinese),
                 "其他");

    CHECK_STR_EQ(localizedStorageErrorLabel("SD init/format failed",
                                           UiLanguage::SimplifiedChinese),
                 "TF 初始化/格式化失败");
    CHECK_STR_EQ(localizedStorageErrorLabel("SD SPI init failed",
                                           UiLanguage::SimplifiedChinese),
                 "TF SPI 初始化失败");
    CHECK_STR_EQ(localizedStorageErrorLabel("No TF card detected",
                                           UiLanguage::SimplifiedChinese),
                 "未检测到 TF 卡");
    CHECK_STR_EQ(localizedStorageErrorLabel("future storage error",
                                           UiLanguage::SimplifiedChinese),
                 "future storage error");
}

TEST_CASE(wifi_recovery_tries_the_existing_link_before_scanning) {
    WifiRecoveryPolicy policy;
    policy.noteLinkLost(1000);

    CHECK_EQ(policy.takeDueAction(2999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(3000), WifiRecoveryAction::ReconnectLast);
    CHECK_EQ(policy.takeDueAction(3001), WifiRecoveryAction::None);

    policy.noteAttemptFailed(4000);
    CHECK_EQ(policy.takeDueAction(8999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(9000), WifiRecoveryAction::ScanProfiles);
}

TEST_CASE(wifi_recovery_scan_backoff_is_bounded_and_wrap_safe) {
    WifiRecoveryPolicy policy;
    policy.noteScanFailed(1000);
    CHECK_EQ(policy.takeDueAction(2999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(3000), WifiRecoveryAction::ScanProfiles);

    policy.noteScanFailed(3000);
    CHECK_EQ(policy.takeDueAction(7999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(8000), WifiRecoveryAction::ScanProfiles);

    policy.noteScanFailed(8000);
    CHECK_EQ(policy.takeDueAction(22999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(23000), WifiRecoveryAction::ScanProfiles);

    policy.noteScanFailed(23000);
    CHECK_EQ(policy.takeDueAction(52999), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(53000), WifiRecoveryAction::ScanProfiles);

    policy.noteScanFailed(0xFFFFFF00u);
    CHECK_EQ(policy.takeDueAction(0x0000742Fu), WifiRecoveryAction::None);
    CHECK_EQ(policy.takeDueAction(0x00007430u), WifiRecoveryAction::ScanProfiles);
}

TEST_CASE(wifi_recovery_success_cancels_pending_work) {
    WifiRecoveryPolicy policy;
    policy.noteLinkLost(1000);
    policy.noteConnected();
    CHECK_EQ(policy.takeDueAction(60000), WifiRecoveryAction::None);

    policy.noteScanFailed(70000);
    policy.reset();
    CHECK_EQ(policy.takeDueAction(100000), WifiRecoveryAction::None);
}

TEST_CASE(wifi_profiles_keep_eight_recent_networks_without_exposing_passwords) {
    WifiProfiles profiles;
    CHECK(profiles.empty());
    CHECK(profiles.upsert("Home", "home-password"));
    CHECK(profiles.upsert("Office", "office-password"));
    CHECK_EQ(profiles.size(), 2u);
    CHECK_STR_EQ(profiles.at(0).ssid.data(), "Office");
    CHECK_STR_EQ(profiles.at(1).ssid.data(), "Home");

    CHECK(profiles.touch("Home"));
    CHECK_STR_EQ(profiles.at(0).ssid.data(), "Home");
    CHECK(profiles.upsert("Office", "new-office-password"));
    CHECK_STR_EQ(profiles.at(0).ssid.data(), "Office");
    CHECK_STR_EQ(profiles.at(0).password.data(), "new-office-password");

    for (int index = 0; index < 9; ++index) {
        char ssid[12];
        std::snprintf(ssid, sizeof(ssid), "Network-%d", index);
        CHECK(profiles.upsert(ssid, "password"));
    }
    CHECK_EQ(profiles.size(), kWifiProfileCapacity);
    CHECK(profiles.find("Network-0") == nullptr);
    CHECK(profiles.find("Network-1") != nullptr);
    CHECK_STR_EQ(profiles.at(0).ssid.data(), "Network-8");

    CHECK(profiles.erase("Network-4"));
    CHECK(profiles.find("Network-4") == nullptr);
    CHECK_EQ(profiles.size(), kWifiProfileCapacity - 1);
    CHECK(!profiles.erase("missing"));
    profiles.clear();
    CHECK(profiles.empty());
}

TEST_CASE(wifi_profiles_reject_empty_or_unterminated_credentials) {
    WifiProfiles profiles;
    CHECK(!profiles.upsert("", "password"));
    CHECK(!profiles.upsert("Home", nullptr));
    std::array<char, kWifiSsidCapacity> unterminatedSsid{};
    unterminatedSsid.fill('x');
    CHECK(!profiles.upsert(unterminatedSsid.data(), "password"));
    std::array<char, kWifiPasswordCapacity> unterminatedPassword{};
    unterminatedPassword.fill('x');
    CHECK(!profiles.upsert("Home", unterminatedPassword.data()));
}

TEST_CASE(ssh_hosts_keep_six_recent_endpoints) {
    SshHosts hosts;
    CHECK(hosts.empty());
    CHECK(hosts.upsert("Pi", "192.0.2.10", "deck", 22));
    CHECK(hosts.upsert("Server", "example.test", "admin", 2222));
    CHECK_EQ(hosts.size(), 2u);
    CHECK_STR_EQ(hosts.at(0).label.data(), "Server");
    CHECK_EQ(hosts.at(0).port, 2222u);

    CHECK(hosts.upsert("Pi updated", "192.0.2.10", "deck", 22));
    CHECK_EQ(hosts.size(), 2u);
    CHECK_STR_EQ(hosts.at(0).label.data(), "Pi updated");

    for (int index = 0; index < 7; ++index) {
        char label[12];
        char hostname[20];
        std::snprintf(label, sizeof(label), "Host %d", index);
        std::snprintf(hostname, sizeof(hostname), "host-%d.test", index);
        CHECK(hosts.upsert(label, hostname, "deck", 22));
    }
    CHECK_EQ(hosts.size(), kSshHostCapacity);
    CHECK_STR_EQ(hosts.at(0).hostname.data(), "host-6.test");
    CHECK(hosts.find("host-0.test", "deck", 22) == nullptr);
    CHECK(hosts.find("host-1.test", "deck", 22) != nullptr);
}

TEST_CASE(ssh_hosts_reject_blank_or_unterminated_fields) {
    SshHosts hosts;
    CHECK(!hosts.upsert("   ", "example.test", "deck", 22));
    CHECK(!hosts.upsert("Server", "   ", "deck", 22));
    CHECK(!hosts.upsert("Server", "example.test", "   ", 22));
    CHECK(!hosts.upsert("Server", "example.test", "deck", 0));

    std::array<char, kSshHostnameCapacity> unterminated{};
    unterminated.fill('x');
    CHECK(!hosts.upsert("Server", unterminated.data(), "deck", 22));
}

TEST_CASE(ssh_hosts_support_edit_delete_and_recent_order) {
    SshHosts hosts;
    CHECK(hosts.upsert("Pi", "pi.test", "deck", 22));
    CHECK(hosts.upsert("Server", "server.test", "admin", 22));
    CHECK(hosts.update(1, "Pi Lab", "pi.test", "deck", 2200));
    CHECK_STR_EQ(hosts.at(1).label.data(), "Pi Lab");
    CHECK_EQ(hosts.at(1).port, 2200u);

    CHECK(hosts.touch(1));
    CHECK_STR_EQ(hosts.at(0).label.data(), "Pi Lab");
    CHECK(hosts.erase(0));
    CHECK_EQ(hosts.size(), 1u);
    CHECK_STR_EQ(hosts.at(0).label.data(), "Server");
    CHECK(!hosts.touch(3));
    CHECK(!hosts.erase(3));
}

TEST_CASE(ssh_host_record_round_trips_and_rejects_corruption) {
    SshHosts hosts;
    CHECK(hosts.upsert("Pi", "pi.test", "deck", 22));
    CHECK(hosts.upsert("Lab", "lab.test", "root", 2200));
    SshHostRecord record = encodeSshHosts(hosts);

    SshHosts decoded;
    CHECK(decodeSshHosts(record, decoded));
    CHECK_EQ(decoded.size(), 2u);
    CHECK_STR_EQ(decoded.at(0).label.data(), "Lab");
    CHECK_STR_EQ(decoded.at(1).hostname.data(), "pi.test");

    record.checksum ^= 0x01u;
    CHECK(!decodeSshHosts(record, decoded));
}

TEST_CASE(ssh_runtime_budget_fits_observed_cardputer_heap) {
    constexpr std::size_t observedFreeHeap = 64072;
    constexpr std::size_t observedLargestBlock = 49140;
    constexpr std::size_t rtosObjectOverheadAllowance = 2048;
    constexpr std::size_t minimumSessionHeapReserve = 32 * 1024;
    constexpr std::size_t observedPeakStackUse = 14 * 1024;
    constexpr std::size_t minimumStackHeadroom = 6 * 1024;
    constexpr std::size_t startupAllocation =
        ssh_memory::kTaskStackBytes + ssh_memory::kTransmitCapacity +
        ssh_memory::kReceiveCapacity + rtosObjectOverheadAllowance;

    CHECK(ssh_memory::kTaskStackBytes <= observedLargestBlock);
    CHECK(ssh_memory::kTaskStackBytes >= observedPeakStackUse + minimumStackHeadroom);
    CHECK(startupAllocation + minimumSessionHeapReserve <= observedFreeHeap);
}

TEST_CASE(display_back_buffer_preserves_ssh_authentication_headroom) {
    constexpr std::size_t rgb565Bytes =
        static_cast<std::size_t>(config::kScreenWidth) * config::kScreenHeight * 2u;
    constexpr std::size_t configuredBytes =
        static_cast<std::size_t>(config::kScreenWidth) * config::kScreenHeight *
            config::kDisplayColorDepth / 8u +
        config::kDisplayPaletteBytes;
    constexpr std::size_t minimumRecoveredHeap = 24u * 1024u;

    CHECK_EQ(config::kDisplayColorDepth, 8u);
    CHECK(rgb565Bytes > configuredBytes);
    CHECK(rgb565Bytes - configuredBytes >= minimumRecoveredHeap);
}

TEST_CASE(indexed_display_palette_has_no_conflicting_color_indices) {
    std::array<uint16_t, theme::kUiPalette.size() + theme::kAnsiPalette.size()> colors{};
    std::size_t count = 0;
    for (const uint16_t color : theme::kUiPalette) colors[count++] = color;
    for (const uint16_t color : theme::kAnsiPalette) colors[count++] = color;

    for (std::size_t left = 0; left < count; ++left) {
        for (std::size_t right = left + 1; right < count; ++right) {
            if ((colors[left] & 0xFFu) == (colors[right] & 0xFFu)) {
                CHECK_EQ(colors[left], colors[right]);
            }
        }
    }
    CHECK_EQ(theme::kAnsiPalette[15], theme::kText);
}

TEST_CASE(terminal_buffer_writes_text_and_tracks_crlf_cursor) {
    TerminalBuffer terminal;
    terminal.write("hello\r\nworld");
    CHECK_EQ(terminal.cell(0, 0).character, 'h');
    CHECK_EQ(terminal.cell(0, 4).character, 'o');
    CHECK_EQ(terminal.cell(1, 0).character, 'w');
    CHECK_EQ(terminal.cell(1, 4).character, 'd');
    CHECK_EQ(terminal.cursorRow(), 1u);
    CHECK_EQ(terminal.cursorColumn(), 5u);
}

TEST_CASE(terminal_buffer_scrolls_and_exposes_local_history) {
    TerminalBuffer terminal;
    for (int line = 0; line < 15; ++line) {
        char text[8];
        std::snprintf(text, sizeof(text), line == 14 ? "%02d" : "%02d\r\n", line);
        terminal.write(text);
    }
    CHECK_EQ(terminal.scrollbackLines(), 2u);
    CHECK_EQ(terminal.cell(0, 0).character, '0');
    CHECK_EQ(terminal.cell(0, 1).character, '2');
    CHECK_EQ(terminal.cell(kTerminalRows - 1, 1).character, '4');

    CHECK(terminal.scrollUp(2));
    CHECK_EQ(terminal.scrollOffset(), 2u);
    CHECK_EQ(terminal.cell(0, 1).character, '0');
    CHECK(terminal.scrollDown(1));
    CHECK_EQ(terminal.cell(0, 1).character, '1');
}

TEST_CASE(terminal_buffer_handles_ansi_cursor_motion_and_line_erase) {
    TerminalBuffer terminal;
    terminal.write("abcdef\x1b[3DXY");
    CHECK_EQ(terminal.cell(0, 2).character, 'c');
    CHECK_EQ(terminal.cell(0, 3).character, 'X');
    CHECK_EQ(terminal.cell(0, 4).character, 'Y');
    CHECK_EQ(terminal.cell(0, 5).character, 'f');

    terminal.write("\x1b[2K");
    for (std::size_t column = 0; column < kTerminalColumns; ++column) {
        CHECK_EQ(terminal.cell(0, column).character, ' ');
    }
}

TEST_CASE(terminal_buffer_handles_ansi_clear_and_absolute_position) {
    TerminalBuffer terminal;
    terminal.write("junk\r\nmore");
    terminal.write("\x1b[2J\x1b[2;3Hok");
    CHECK_EQ(terminal.cell(0, 0).character, ' ');
    CHECK_EQ(terminal.cell(1, 0).character, ' ');
    CHECK_EQ(terminal.cell(1, 2).character, 'o');
    CHECK_EQ(terminal.cell(1, 3).character, 'k');
    CHECK_EQ(terminal.cursorRow(), 1u);
    CHECK_EQ(terminal.cursorColumn(), 4u);
}

TEST_CASE(terminal_buffer_preserves_basic_ansi_colors_per_cell) {
    TerminalBuffer terminal;
    terminal.write("\x1b[31;44mR\x1b[0mN");
    CHECK_EQ(terminal.cell(0, 0).foreground(), 1u);
    CHECK_EQ(terminal.cell(0, 0).background(), 4u);
    CHECK_EQ(terminal.cell(0, 1).foreground(), 7u);
    CHECK_EQ(terminal.cell(0, 1).background(), 0u);
}

TEST_CASE(terminal_buffer_handles_relative_cursor_motion_across_writes) {
    TerminalBuffer terminal;
    terminal.write("\x1b[3;5H");
    terminal.write("X\x1b[");
    terminal.write("2A\x1b[3CY");
    CHECK_EQ(terminal.cell(2, 4).character, 'X');
    CHECK_EQ(terminal.cell(0, 8).character, 'Y');
    CHECK_EQ(terminal.cursorRow(), 0u);
    CHECK_EQ(terminal.cursorColumn(), 9u);
}

TEST_CASE(terminal_buffer_handles_backspace_and_tab_stops) {
    TerminalBuffer terminal;
    terminal.write("abc\bZ\tQ");
    CHECK_EQ(terminal.cell(0, 0).character, 'a');
    CHECK_EQ(terminal.cell(0, 1).character, 'b');
    CHECK_EQ(terminal.cell(0, 2).character, 'Z');
    CHECK_EQ(terminal.cell(0, 8).character, 'Q');
    CHECK_EQ(terminal.cursorColumn(), 9u);
}

TEST_CASE(terminal_buffer_ignores_shell_title_and_charset_sequences) {
    TerminalBuffer terminal;
    terminal.write("\x1b]0;private-title\x07prompt");
    terminal.write("\x1b(B ok");
    CHECK_EQ(terminal.cell(0, 0).character, 'p');
    CHECK_EQ(terminal.cell(0, 5).character, 't');
    CHECK_EQ(terminal.cell(0, 6).character, ' ');
    CHECK_EQ(terminal.cell(0, 7).character, 'o');
    CHECK_EQ(terminal.cell(0, 8).character, 'k');
    CHECK_EQ(terminal.cursorColumn(), 9u);
}

TEST_CASE(ssh_app_host_list_navigates_and_emits_explicit_actions) {
    SshAppModel model;
    CHECK_EQ(model.page(), SshPage::HostList);
    CHECK_EQ(model.inputMode(), InputMode::Text);
    model.handle({InputAction::Down, '\0'}, 3);
    CHECK_EQ(model.selectedHost(), 1u);
    auto result = model.handle({InputAction::None, 'e'}, 3);
    CHECK_EQ(result.effect, SshEffect::EditSelected);
    CHECK_EQ(result.index, 1u);
    result = model.handle({InputAction::None, 'd'}, 3);
    CHECK_EQ(model.page(), SshPage::ConfirmDelete);
    CHECK_EQ(result.effect, SshEffect::None);
    result = model.handle({InputAction::Back, '\0'}, 3);
    CHECK_EQ(model.page(), SshPage::HostList);

    result = model.handle({InputAction::Confirm, '\0'}, 3);
    CHECK_EQ(result.effect, SshEffect::ConnectSelected);
    CHECK_EQ(result.index, 1u);
    result = model.handle({InputAction::Erase, '\0'}, 3);
    CHECK_EQ(result.effect, SshEffect::GoHome);
}

TEST_CASE(ssh_app_editor_builds_a_valid_host_without_heap_input) {
    SshAppModel model;
    model.handle({InputAction::None, 'n'}, 0);
    CHECK_EQ(model.page(), SshPage::Editor);
    CHECK_STR_EQ(model.editorValue(3), "22");

    for (const char character : std::array<char, 2>{'P', 'i'}) {
        model.handle({InputAction::None, character}, 0);
    }
    model.handle({InputAction::Tab, '\0'}, 0);
    for (const char character : std::array<char, 7>{'p', 'i', '.', 't', 'e', 's', 't'}) {
        model.handle({InputAction::None, character}, 0);
    }
    model.handle({InputAction::Tab, '\0'}, 0);
    for (const char character : std::array<char, 4>{'d', 'e', 'c', 'k'}) {
        model.handle({InputAction::None, character}, 0);
    }
    model.handle({InputAction::Tab, '\0'}, 0);
    const auto result = model.handle({InputAction::Confirm, '\0'}, 0);
    CHECK_EQ(result.effect, SshEffect::SaveEditor);

    SshHost host;
    CHECK(model.editedHost(host));
    CHECK_STR_EQ(host.label.data(), "Pi");
    CHECK_STR_EQ(host.hostname.data(), "pi.test");
    CHECK_STR_EQ(host.username.data(), "deck");
    CHECK_EQ(host.port, 22u);
}

TEST_CASE(ssh_app_editor_can_load_and_update_an_existing_host) {
    SshHosts hosts;
    CHECK(hosts.upsert("Lab", "lab.test", "root", 22));
    SshAppModel model;
    model.beginEdit(hosts.at(0), 0);
    CHECK_EQ(model.page(), SshPage::Editor);
    CHECK_EQ(model.editingIndex(), 0u);
    CHECK_STR_EQ(model.editorValue(0), "Lab");
    CHECK_STR_EQ(model.editorValue(1), "lab.test");
    CHECK_STR_EQ(model.editorValue(2), "root");
    CHECK_STR_EQ(model.editorValue(3), "22");

    model.handle({InputAction::Back, '\0'}, 1);
    CHECK_EQ(model.page(), SshPage::HostList);
}

TEST_CASE(ssh_app_editor_stays_open_and_marks_invalid_records) {
    SshAppModel model;
    model.handle({InputAction::None, 'n'}, 0);
    model.handle({InputAction::Tab, '\0'}, 0);
    model.handle({InputAction::Tab, '\0'}, 0);
    model.handle({InputAction::Tab, '\0'}, 0);
    const auto result = model.handle({InputAction::Confirm, '\0'}, 0);
    CHECK_EQ(result.effect, SshEffect::None);
    CHECK_EQ(model.page(), SshPage::Editor);
    CHECK(model.editorHasError());
}

TEST_CASE(ssh_app_session_pages_support_cancel_reconnect_and_quick_commands) {
    SshAppModel model;
    model.showConnecting();
    CHECK_EQ(model.page(), SshPage::Connecting);
    auto result = model.handle({InputAction::Back, '\0'}, 1);
    CHECK_EQ(result.effect, SshEffect::CancelConnection);
    CHECK_EQ(model.page(), SshPage::HostList);

    model.showTerminal();
    CHECK_EQ(model.inputMode(), InputMode::Terminal);
    model.handle({InputAction::QuickCommands, '\0'}, 1);
    CHECK_EQ(model.page(), SshPage::QuickCommands);
    CHECK_EQ(model.inputMode(), InputMode::System);
    model.handle({InputAction::Down, '\0'}, 1);
    CHECK_STR_EQ(model.quickCommand(), "df -h");
    result = model.handle({InputAction::Confirm, '\0'}, 1);
    CHECK_EQ(result.effect, SshEffect::SendQuickCommand);
    CHECK_EQ(model.page(), SshPage::Terminal);

    model.showDisconnected();
    result = model.handle({InputAction::Confirm, '\0'}, 1);
    CHECK_EQ(result.effect, SshEffect::Reconnect);
}

TEST_CASE(ssh_app_requires_confirmation_before_deleting_a_host) {
    SshAppModel model;
    model.handle({InputAction::Down, '\0'}, 2);
    model.handle({InputAction::None, 'd'}, 2);
    CHECK_EQ(model.page(), SshPage::ConfirmDelete);
    auto result = model.handle({InputAction::Back, '\0'}, 2);
    CHECK_EQ(result.effect, SshEffect::None);
    CHECK_EQ(model.page(), SshPage::HostList);

    model.handle({InputAction::None, 'd'}, 2);
    result = model.handle({InputAction::Confirm, '\0'}, 2);
    CHECK_EQ(result.effect, SshEffect::DeleteSelected);
    CHECK_EQ(result.index, 1u);
    CHECK_EQ(model.page(), SshPage::HostList);
    CHECK_EQ(model.selectedHost(), 0u);
}

TEST_CASE(local_clock_rejects_unsynced_time_and_applies_utc_offset) {
    constexpr int64_t midnightUtc = 1704067200;  // 2024-01-01 00:00 UTC
    CHECK(!clockFromUtcEpoch(0, 8 * 60 * 60).valid);

    const ClockDisplay morning = clockFromUtcEpoch(midnightUtc, 8 * 60 * 60);
    CHECK(morning.valid);
    CHECK_EQ(morning.hour, 8);
    CHECK_EQ(morning.minute, 0);

    const ClockDisplay nextDay =
        clockFromUtcEpoch(midnightUtc + 16 * 60 * 60 + 30 * 60, 8 * 60 * 60);
    CHECK_EQ(nextDay.hour, 0);
    CHECK_EQ(nextDay.minute, 30);
}

TEST_CASE(weather_display_keeps_successful_data_when_inputs_disappear) {
    WeatherSnapshot weather;
    CHECK_EQ(classifyWeatherDisplay(weather, false, false, false),
             WeatherDisplayState::WifiOff);
    CHECK_EQ(classifyWeatherDisplay(weather, true, true, false),
             WeatherDisplayState::WaitingGps);

    weather.state = WeatherState::Fetching;
    CHECK_EQ(classifyWeatherDisplay(weather, true, true, true),
             WeatherDisplayState::Fetching);

    weather.valid = true;
    weather.state = WeatherState::Ready;
    CHECK_EQ(classifyWeatherDisplay(weather, true, true, true),
             WeatherDisplayState::Live);
    const WeatherDisplayState noGps =
        classifyWeatherDisplay(weather, true, true, false);
    CHECK_EQ(noGps, WeatherDisplayState::CachedNoGps);
    CHECK(weatherDisplayShowsData(noGps));
    const WeatherDisplayState offline =
        classifyWeatherDisplay(weather, false, false, false);
    CHECK_EQ(offline, WeatherDisplayState::CachedOffline);
    CHECK(weatherDisplayShowsData(offline));

    weather.state = WeatherState::Fetching;
    CHECK_EQ(classifyWeatherDisplay(weather, true, true, true),
             WeatherDisplayState::Updating);
    weather.state = WeatherState::Error;
    CHECK_EQ(classifyWeatherDisplay(weather, true, true, true),
             WeatherDisplayState::CachedError);
    CHECK(weatherDisplayShowsData(WeatherDisplayState::CachedError));
}

TEST_CASE(gps_state_distinguishes_stream_search_fix_and_stale) {
    GpsSnapshot snapshot;
    CHECK_EQ(classifyGpsState(snapshot), GpsState::NoData);

    snapshot.charsProcessed = 120;
    snapshot.dataAgeMs = 50;
    CHECK_EQ(classifyGpsState(snapshot), GpsState::Searching);

    snapshot.locationValid = true;
    snapshot.locationAgeMs = 250;
    CHECK_EQ(classifyGpsState(snapshot), GpsState::Fix);

    snapshot.locationAgeMs = 6000;
    CHECK_EQ(classifyGpsState(snapshot), GpsState::Stale);

    snapshot.dataAgeMs = 4000;
    CHECK_EQ(classifyGpsState(snapshot), GpsState::NoStream);
}

TEST_CASE(gps_page_navigation_wraps_across_all_four_pages) {
    GpsAppModel model;
    CHECK_EQ(model.page(), GpsPage::Position);

    model.handle(InputAction::Left);
    CHECK_EQ(model.page(), GpsPage::Motion);
    model.handle(InputAction::Left);
    CHECK_EQ(model.page(), GpsPage::Receiver);
    model.handle(InputAction::Left);
    CHECK_EQ(model.page(), GpsPage::Time);
    model.handle(InputAction::Left);
    CHECK_EQ(model.page(), GpsPage::Position);

    model.handle(InputAction::Right);
    CHECK_EQ(model.page(), GpsPage::Time);
    model.handle(InputAction::Right);
    CHECK_EQ(model.page(), GpsPage::Receiver);
    model.handle(InputAction::Right);
    CHECK_EQ(model.page(), GpsPage::Motion);
    model.handle(InputAction::Right);
    CHECK_EQ(model.page(), GpsPage::Position);

    model.handle(InputAction::Tab);
    CHECK_EQ(model.page(), GpsPage::Time);
    model.handle(InputAction::Tab);
    CHECK_EQ(model.page(), GpsPage::Receiver);
    model.handle(InputAction::Tab);
    CHECK_EQ(model.page(), GpsPage::Motion);
    model.handle(InputAction::Tab);
    CHECK_EQ(model.page(), GpsPage::Position);
}

TEST_CASE(gps_compass_points_cover_cardinal_and_intercardinal_directions) {
    CHECK_STR_EQ(gpsCompassPoint(0.0), "N");
    CHECK_STR_EQ(gpsCompassPoint(44.9), "NE");
    CHECK_STR_EQ(gpsCompassPoint(90.0), "E");
    CHECK_STR_EQ(gpsCompassPoint(180.0), "S");
    CHECK_STR_EQ(gpsCompassPoint(270.0), "W");
    CHECK_STR_EQ(gpsCompassPoint(359.9), "N");
    CHECK_STR_EQ(gpsCompassPoint(-90.0), "W");
}

TEST_CASE(gps_fix_quality_and_mode_have_readable_labels) {
    CHECK_STR_EQ(gpsFixQualityLabel('0'), "NONE");
    CHECK_STR_EQ(gpsFixQualityLabel('1'), "GPS");
    CHECK_STR_EQ(gpsFixQualityLabel('2'), "DGPS");
    CHECK_STR_EQ(gpsFixQualityLabel('4'), "RTK");
    CHECK_STR_EQ(gpsFixQualityLabel('?'), "--");
    CHECK_STR_EQ(gpsFixModeLabel('A'), "AUTON");
    CHECK_STR_EQ(gpsFixModeLabel('D'), "DIFF");
    CHECK_STR_EQ(gpsFixModeLabel('N'), "NONE");
}

TEST_CASE(gps_localized_text_covers_states_compass_quality_and_mode) {
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::Fix, UiLanguage::English), "FIX");
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::NoData,
                                       UiLanguage::SimplifiedChinese),
                 "无数据");
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::NoStream,
                                       UiLanguage::SimplifiedChinese),
                 "数据中断");
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::Searching,
                                       UiLanguage::SimplifiedChinese),
                 "正在搜星");
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::Stale,
                                       UiLanguage::SimplifiedChinese),
                 "定位过期");
    CHECK_STR_EQ(localizedGpsStateLabel(GpsState::Fix,
                                       UiLanguage::SimplifiedChinese),
                 "已定位");

    CHECK_STR_EQ(localizedGpsCompassPoint(45.0, UiLanguage::English), "NE");
    CHECK_STR_EQ(localizedGpsCompassPoint(0.0, UiLanguage::SimplifiedChinese), "北");
    CHECK_STR_EQ(localizedGpsCompassPoint(45.0, UiLanguage::SimplifiedChinese),
                 "东北");
    CHECK_STR_EQ(localizedGpsCompassPoint(225.0, UiLanguage::SimplifiedChinese),
                 "西南");
    CHECK_STR_EQ(localizedGpsCompassPoint(-90.0, UiLanguage::SimplifiedChinese),
                 "西");

    CHECK_STR_EQ(localizedGpsFixQualityLabel('4', UiLanguage::English), "RTK");
    CHECK_STR_EQ(localizedGpsFixQualityLabel('5', UiLanguage::SimplifiedChinese),
                 "浮点");
    CHECK_STR_EQ(localizedGpsFixQualityLabel('8', UiLanguage::SimplifiedChinese),
                 "模拟");
    CHECK_STR_EQ(localizedGpsFixModeLabel('A', UiLanguage::SimplifiedChinese),
                 "自主");
    CHECK_STR_EQ(localizedGpsFixModeLabel('D', UiLanguage::SimplifiedChinese),
                 "差分");
}

TEST_CASE(single_host_policy_allows_pairing_only_without_a_stored_bond) {
    CHECK(BleKeyboardPolicy::newPairingAllowed(false));
    CHECK(!BleKeyboardPolicy::newPairingAllowed(true));
}

TEST_CASE(ble_advertising_deadline_handles_millis_wraparound) {
    CHECK(!BleKeyboardPolicy::deadlineReached(999, 1000));
    CHECK(BleKeyboardPolicy::deadlineReached(1000, 1000));
    CHECK(BleKeyboardPolicy::deadlineReached(1001, 1000));
    CHECK(!BleKeyboardPolicy::deadlineReached(0xFFFFFFF0u, 0x00000010u));
    CHECK(BleKeyboardPolicy::deadlineReached(0x00000010u, 0xFFFFFFF0u));
}

TEST_CASE(ble_report_gate_sends_release_then_deduplicates) {
    BleKeyboardPolicy policy;
    const HidReport letterA = HidReport::single(0, 0x04);
    policy.setConnected(true);

    auto decision = policy.nextReport(letterA);
    CHECK(decision.send);
    CHECK(decision.report.empty());

    decision = policy.nextReport(letterA);
    CHECK(decision.send);
    CHECK_EQ(decision.report, letterA);

    decision = policy.nextReport(letterA);
    CHECK(!decision.send);

    decision = policy.nextReport(HidReport{});
    CHECK(decision.send);
    CHECK(decision.report.empty());
}

TEST_CASE(disconnect_blocks_reports_and_resets_gate) {
    BleKeyboardPolicy policy;
    policy.setConnected(true);
    policy.nextReport(HidReport{});
    policy.setConnected(false);
    CHECK(!policy.nextReport(HidReport::single(0, 0x04)).send);
    policy.setConnected(true);
    const auto decision = policy.nextReport(HidReport::single(0, 0x04));
    CHECK(decision.send);
    CHECK(decision.report.empty());
}

TEST_CASE(diagnostics_ring_is_bounded_and_newest_first) {
    DiagnosticsService diagnostics;
    for (int index = 0; index < 14; ++index) diagnostics.logf("event %d", index);

    CHECK_EQ(diagnostics.size(), DiagnosticsService::kCapacity);
    CHECK_STR_EQ(diagnostics.newest(0), "event 13");
    CHECK_STR_EQ(diagnostics.newest(11), "event 2");
    CHECK_STR_EQ(diagnostics.newest(12), "");
}

TEST_CASE(diagnostics_messages_are_safely_truncated) {
    DiagnosticsService diagnostics;
    diagnostics.log("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    CHECK_EQ(std::char_traits<char>::length(diagnostics.newest(0)),
             DiagnosticsService::kMessageCapacity - 1);
}

TEST_CASE(diagnostics_sink_replays_history_and_keeps_full_messages) {
    DiagnosticsService diagnostics;
    diagnostics.log("before sink");
    DiagnosticSinkCapture capture;
    diagnostics.setSink(&captureDiagnostic, &capture, true);
    CHECK_EQ(capture.count, 1u);
    CHECK_STR_EQ(capture.messages[0].data(), "before sink");

    constexpr const char* longMessage =
        "BLE encrypted addr=AA:BB:CC:DD:EE:FF type=1 bond=1 auth=0x0d key=0";
    diagnostics.log(longMessage);
    CHECK_EQ(capture.count, 2u);
    CHECK_STR_EQ(capture.messages[1].data(), longMessage);
    CHECK_EQ(std::char_traits<char>::length(diagnostics.newest(0)),
             DiagnosticsService::kMessageCapacity - 1);
}

TEST_CASE(async_diagnostics_are_drained_into_the_persistent_sink) {
    DiagnosticsService diagnostics;
    DiagnosticSinkCapture capture;
    diagnostics.setSink(&captureDiagnostic, &capture);
    CHECK(diagnostics.enqueue("BLE disconnect reason=0x08"));
    CHECK(diagnostics.enqueue("BLE advertising start requested"));
    CHECK_EQ(capture.count, 0u);
    diagnostics.drainPending();
    CHECK_EQ(capture.count, 2u);
    CHECK_STR_EQ(capture.messages[0].data(), "BLE disconnect reason=0x08");
    CHECK_STR_EQ(capture.messages[1].data(), "BLE advertising start requested");
}

TEST_CASE(serial_log_commands_are_normalized_and_require_clear_confirmation) {
    CHECK_EQ(parseSerialCommand(nullptr), SerialCommand::None);
    CHECK_EQ(parseSerialCommand(""), SerialCommand::None);
    CHECK_EQ(parseSerialCommand("  log   status  "), SerialCommand::LogStatus);
    CHECK_EQ(parseSerialCommand("LOG DUMP"), SerialCommand::LogDump);
    CHECK_EQ(parseSerialCommand("log dump all"), SerialCommand::LogDumpAll);
    CHECK_EQ(parseSerialCommand("LOG CLEAR"), SerialCommand::Unknown);
    CHECK_EQ(parseSerialCommand("LOG CLEAR YES"), SerialCommand::LogClearConfirmed);
    CHECK_EQ(parseSerialCommand("help"), SerialCommand::Help);
}

TEST_CASE(quick_settings_adjusts_values_and_clamps) {
    QuickSettingsModel quick;
    quick.open({95, 5, true});
    auto result = quick.handle(InputAction::Right);
    CHECK(result.valuesChanged);
    CHECK_EQ(quick.values().brightness, 100);
    result = quick.handle(InputAction::Down);
    CHECK_EQ(quick.values().volume, 0);
    result = quick.handle(InputAction::Confirm);
    CHECK(!quick.values().bleEnabled);
    CHECK(quick.dirty());
}

TEST_CASE(quick_settings_close_reports_whether_persistence_is_needed) {
    QuickSettingsModel quick;
    quick.open({78, 55, true});
    auto result = quick.handle(InputAction::Back);
    CHECK(result.closed);
    CHECK(!result.persist);

    quick.open({78, 55, true});
    quick.handle(InputAction::Left);
    result = quick.close();
    CHECK(result.closed);
    CHECK(result.persist);
    CHECK(!quick.active());
}

TEST_CASE(settings_categories_open_and_back_out) {
    SettingsModel model;
    CHECK_EQ(model.page(), SettingsPage::Categories);
    CHECK_EQ(model.category(), SettingsCategory::Wifi);
    model.handle(InputAction::Down);
    CHECK_EQ(model.category(), SettingsCategory::Bluetooth);
    model.handle(InputAction::Down);
    CHECK_EQ(model.category(), SettingsCategory::System);
    model.handle(InputAction::Confirm);
    CHECK_EQ(model.page(), SettingsPage::System);
    model.handle(InputAction::Back);
    CHECK_EQ(model.page(), SettingsPage::Categories);
    const auto result = model.handle(InputAction::Back);
    CHECK_EQ(result.effect, SettingsEffect::GoHome);
}

TEST_CASE(settings_destructive_actions_require_confirmation) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::ConfirmForgetHost);
    result = model.handle(InputAction::Back);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::Bluetooth);

    model.handle(InputAction::Confirm);
    CHECK_EQ(model.page(), SettingsPage::ConfirmForgetHost);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::ForgetHost);
    CHECK_EQ(model.page(), SettingsPage::Bluetooth);
}

TEST_CASE(settings_bluetooth_controls_emit_explicit_effects) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::ToggleBluetooth);
    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::DisconnectBluetooth);
}

TEST_CASE(settings_wifi_scan_selection_and_forget_are_explicit) {
    SettingsModel model;
    model.handle(InputAction::Confirm);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::ToggleWifi);

    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::StartWifiScan);
    CHECK_EQ(model.page(), SettingsPage::WifiNetworks);

    model.handle(InputAction::Down, 3);
    CHECK_EQ(model.selectedRow(), 1);
    result = model.handle(InputAction::Confirm, 3);
    CHECK_EQ(result.effect, SettingsEffect::SelectWifiNetwork);
    model.openWifiPassword();
    CHECK_EQ(model.page(), SettingsPage::WifiPassword);
    model.cancelWifiPassword();
    CHECK_EQ(model.page(), SettingsPage::WifiNetworks);
    model.handle(InputAction::Back, 3);
    CHECK_EQ(model.page(), SettingsPage::Wifi);

    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm, 0, 2);
    CHECK_EQ(model.page(), SettingsPage::WifiSavedNetworks);
    result = model.handle(InputAction::Confirm, 0, 2);
    CHECK_EQ(result.effect, SettingsEffect::SelectWifiForForget);
    CHECK_EQ(model.page(), SettingsPage::ConfirmForgetWifi);
    result = model.handle(InputAction::Confirm, 0, 2);
    CHECK_EQ(result.effect, SettingsEffect::ForgetWifi);
    CHECK_EQ(model.page(), SettingsPage::WifiSavedNetworks);
}

TEST_CASE(settings_system_actions_and_diagnostics_are_reachable) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::ToggleLanguage);
    CHECK_EQ(model.page(), SettingsPage::System);
    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::Diagnostics);
    model.handle(InputAction::Back);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    CHECK_EQ(model.page(), SettingsPage::ConfirmRestart);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::Restart);
}

TEST_CASE(settings_storage_mount_and_format_are_explicit) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::Storage);

    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::MountStorage);
    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::ConfirmFormatStorage);
    result = model.handle(InputAction::Back);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::Storage);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::ConfirmFormatStorage);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::FormatStorage);
    CHECK_EQ(model.page(), SettingsPage::Storage);
}

TEST_CASE(settings_factory_reset_requires_its_own_confirmation) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::ConfirmFactoryReset);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::FactoryReset);
}

TEST_CASE(lora_draft_accepts_printable_ascii_and_copies_exact_payload) {
    LoRaData model;
    CHECK(model.draftEmpty());
    CHECK(!model.draftFull());
    CHECK(!model.canSend());
    CHECK(model.appendDraft('A'));
    CHECK(model.appendDraft(' '));
    CHECK(model.appendDraft('~'));
    CHECK(!model.appendDraft('\n'));
    CHECK(!model.appendDraft(static_cast<char>(0x7f)));
    CHECK_EQ(model.draftLength(), 3u);
    CHECK_STR_EQ(model.draft(), "A ~");

    std::array<uint8_t, kLoRaPayloadLimit> payload{};
    CHECK_EQ(model.copyDraft(payload.data(), payload.size()), 3u);
    CHECK_EQ(payload[0], static_cast<uint8_t>('A'));
    CHECK_EQ(payload[1], static_cast<uint8_t>(' '));
    CHECK_EQ(payload[2], static_cast<uint8_t>('~'));
    CHECK_EQ(model.copyDraft(payload.data(), 2), 0u);

    CHECK(model.eraseDraft());
    CHECK_STR_EQ(model.draft(), "A ");
    model.clearDraft();
    for (std::size_t index = 0; index < kLoRaPayloadLimit; ++index) {
        CHECK(model.appendDraft('x'));
    }
    CHECK(model.draftFull());
    CHECK(!model.appendDraft('y'));
    CHECK_EQ(model.draftLength(), kLoRaPayloadLimit);
}

TEST_CASE(lora_send_requires_a_listening_nonempty_draft_and_records_exact_tx) {
    LoRaData model;
    CHECK_EQ(model.state(), LoRaRadioState::Unavailable);
    model.beginInitialization();
    CHECK_EQ(model.state(), LoRaRadioState::Initializing);
    model.beginListening();
    CHECK_EQ(model.state(), LoRaRadioState::Listening);
    CHECK(!model.beginTransmit());
    CHECK(model.appendDraft('T'));
    CHECK(model.appendDraft('X'));
    CHECK(model.canSend());
    CHECK(model.beginTransmit());
    CHECK_EQ(model.state(), LoRaRadioState::Transmitting);
    CHECK(model.completeTransmit(0));
    CHECK_EQ(model.state(), LoRaRadioState::Listening);
    CHECK(model.draftEmpty());
    CHECK_EQ(model.counters().sent, 1u);
    CHECK_EQ(model.historySize(), 1u);
    CHECK_EQ(model.historyAt(0).direction, LoRaMessageDirection::Tx);
    CHECK_EQ(model.historyAt(0).length, 2u);
    CHECK_STR_EQ(model.historyAt(0).text.data(), "TX");
}

TEST_CASE(lora_transmitting_draft_is_immutable_until_completion) {
    LoRaData model;
    model.beginListening();
    CHECK(model.appendDraft('O'));
    CHECK(model.appendDraft('K'));

    std::array<uint8_t, kLoRaPayloadLimit> transmitted{};
    CHECK_EQ(model.copyDraft(transmitted.data(), transmitted.size()), 2u);
    CHECK(model.beginTransmit());
    CHECK(!model.appendDraft('!'));
    CHECK(!model.eraseDraft());
    model.clearDraft();
    CHECK_EQ(model.copyDraft(transmitted.data(), transmitted.size()), 2u);
    CHECK_EQ(transmitted[0], static_cast<uint8_t>('O'));
    CHECK_EQ(transmitted[1], static_cast<uint8_t>('K'));
    CHECK(model.completeTransmit(0));
    CHECK_STR_EQ(model.historyAt(0).text.data(), "OK");
    CHECK(model.draftEmpty());
}

TEST_CASE(lora_pending_tx_snapshots_same_frame_payload_and_locks_edits) {
    LoRaTxPolicy policy;
    LoRaData model;
    model.beginListening();
    CHECK(policy.appendDraft(model, 'O'));
    CHECK(policy.appendDraft(model, 'K'));

    CHECK(policy.capture(reinterpret_cast<const uint8_t*>(model.draft()),
                         model.draftLength()));
    CHECK(policy.active());
    CHECK_EQ(policy.length(), 2u);
    CHECK_EQ(policy.payload()[0], static_cast<uint8_t>('O'));
    CHECK_EQ(policy.payload()[1], static_cast<uint8_t>('K'));
    CHECK(!policy.appendDraft(model, '!'));
    CHECK(!policy.eraseDraft(model));
    policy.clearDraft(model);
    CHECK_STR_EQ(model.draft(), "OK");
    CHECK(!policy.capture(policy.payload(), policy.length()));

    CHECK(model.beginTransmit());
    CHECK(model.completeTransmit(0));
    CHECK_STR_EQ(model.historyAt(0).text.data(), "OK");

    policy.clear();
    CHECK(!policy.active());
    CHECK_EQ(policy.length(), 0u);
    CHECK(policy.appendDraft(model, 'R'));
}

TEST_CASE(lora_tx_watchdog_deadline_is_inclusive_and_wrap_safe) {
    LoRaTxPolicy policy;
    const uint8_t payload = 'T';
    CHECK(policy.capture(&payload, 1));

    constexpr uint32_t startMs = 5000;
    constexpr uint32_t timeOnAirUs = 1501;
    constexpr uint32_t durationMs = 2 + LoRaTxPolicy::kWatchdogSafetyMarginMs;
    policy.armWatchdog(startMs, timeOnAirUs);
    CHECK(policy.watchdogArmed());
    CHECK(!policy.watchdogExpired(startMs + durationMs - 1));
    CHECK(policy.watchdogExpired(startMs + durationMs));
    CHECK(policy.watchdogExpired(startMs + durationMs + 1));

    constexpr uint32_t wrapStartMs = UINT32_MAX - 500u;
    const uint32_t wrapDeadlineMs = wrapStartMs + durationMs;
    policy.armWatchdog(wrapStartMs, timeOnAirUs);
    CHECK(!policy.watchdogExpired(wrapDeadlineMs - 1));
    CHECK(policy.watchdogExpired(wrapDeadlineMs));
    CHECK(policy.watchdogExpired(wrapDeadlineMs + 1));

    policy.clear();
    CHECK(!policy.watchdogArmed());
    CHECK(!policy.watchdogExpired(wrapDeadlineMs));
}

TEST_CASE(lora_history_evicts_oldest_and_has_readable_direction_labels) {
    LoRaData model;
    model.beginListening();
    for (uint8_t index = 0; index < 7; ++index) {
        const uint8_t byte[] = {static_cast<uint8_t>('0' + index)};
        CHECK(model.recordReceive(byte, sizeof(byte), -70.0f - index,
                                  7.0f - index, 0));
    }
    CHECK_EQ(model.historySize(), kLoRaHistoryCapacity);
    CHECK_EQ(model.historyAt(0).direction, LoRaMessageDirection::Rx);
    CHECK_STR_EQ(model.historyAt(0).text.data(), "1");
    CHECK_STR_EQ(model.historyAt(kLoRaHistoryCapacity - 1).text.data(), "6");
    CHECK_STR_EQ(loraMessageDirectionLabel(LoRaMessageDirection::Rx), "RX");
    CHECK_STR_EQ(loraMessageDirectionLabel(LoRaMessageDirection::Tx), "TX");
}

TEST_CASE(lora_rx_sanitizes_payload_rejects_oversize_and_tracks_quality) {
    LoRaData model;
    model.beginListening();
    const uint8_t received[] = {'O', 'K', 0x00, 0x1f, 0x7f, '!', 0xff};
    CHECK(model.recordReceive(received, sizeof(received), -87.5f, 6.25f, 0));
    CHECK_EQ(model.counters().received, 1u);
    CHECK_EQ(model.counters().droppedPackets, 0u);
    CHECK(model.hasReceiveQuality());
    CHECK_EQ(model.lastRssi(), -87.5f);
    CHECK_EQ(model.lastSnr(), 6.25f);
    CHECK_STR_EQ(model.historyAt(0).text.data(), "OK...!.");

    std::array<uint8_t, kLoRaPayloadLimit + 1> oversize{};
    CHECK(!model.recordReceive(oversize.data(), oversize.size(), -20.0f, 1.0f, -1));
    CHECK_EQ(model.counters().received, 1u);
    CHECK_EQ(model.counters().droppedPackets, 1u);
    CHECK_EQ(model.historySize(), 1u);
    CHECK_EQ(model.lastRssi(), -87.5f);
    CHECK_EQ(model.lastSnr(), 6.25f);
    CHECK_EQ(model.lastStatusCode(), -1);
}

TEST_CASE(lora_state_transitions_cover_crc_restart_and_persistent_error) {
    LoRaData model;
    model.beginInitialization();
    model.beginListening();
    model.recordCrcFailure(-7);
    CHECK_EQ(model.state(), LoRaRadioState::Listening);
    CHECK_EQ(model.counters().crcFailures, 1u);
    CHECK_EQ(model.lastStatusCode(), -7);

    model.beginRecoverableRestart(-8);
    CHECK_EQ(model.state(), LoRaRadioState::Initializing);
    CHECK_EQ(model.lastStatusCode(), -8);
    model.completeRecoverableRestart(0);
    CHECK_EQ(model.state(), LoRaRadioState::Listening);
    CHECK_EQ(model.lastStatusCode(), 0);
    model.setPersistentError(-9);
    CHECK_EQ(model.state(), LoRaRadioState::Error);
    CHECK_EQ(model.lastStatusCode(), -9);
    CHECK_STR_EQ(loraRadioStateLabel(LoRaRadioState::Error), "ERROR");
}

TEST_CASE(lora_app_lifecycle_emits_only_lazy_start_and_draft_clear) {
    LoRaAppModel model;
    CHECK_EQ(model.enter(), LoRaAppEffect::EnsureStarted);
    CHECK_EQ(model.exit(), LoRaAppEffect::ClearDraft);
}

TEST_CASE(lora_app_maps_text_edit_send_and_home_actions) {
    LoRaAppModel model;
    auto result = model.handle({InputAction::None, 'A'}, LoRaRadioState::Unavailable,
                               true, 10);
    CHECK_EQ(result.effect, LoRaAppEffect::AppendDraft);
    CHECK_EQ(result.character, 'A');

    result = model.handle({InputAction::Erase, '\0'}, LoRaRadioState::Listening,
                          false, 11);
    CHECK_EQ(result.effect, LoRaAppEffect::EraseDraft);
    result = model.handle({InputAction::Back, '\0'}, LoRaRadioState::Listening,
                          false, 12);
    CHECK_EQ(result.effect, LoRaAppEffect::GoHome);

    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Listening,
                          true, 13);
    CHECK_EQ(result.effect, LoRaAppEffect::None);
    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Unavailable,
                          false, 14);
    CHECK_EQ(result.effect, LoRaAppEffect::None);
    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Initializing,
                          false, 14);
    CHECK_EQ(result.effect, LoRaAppEffect::None);
    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Error,
                          false, 14);
    CHECK_EQ(result.effect, LoRaAppEffect::None);
    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Listening,
                          false, 15);
    CHECK_EQ(result.effect, LoRaAppEffect::RequestTransmit);
}

TEST_CASE(lora_app_rejected_send_feedback_is_bounded_visible_and_wrap_safe) {
    LoRaAppModel model;
    model.rejectSend(100);
    CHECK(model.sendRejectedVisible(100));
    CHECK(model.sendRejectedVisible(133));
    CHECK(model.sendRejectedVisible(1099));
    CHECK(!model.sendRejectedVisible(1100));

    model.rejectSend(UINT32_MAX - 20u);
    CHECK(model.sendRejectedVisible(12));
    CHECK(!model.sendRejectedVisible(979));

    model.rejectSend(2000);
    model.handle({InputAction::None, 'x'}, LoRaRadioState::Listening, false, 2001);
    CHECK(!model.sendRejectedVisible(2001));
    model.rejectSend(3000);
    model.handle({InputAction::Erase, '\0'}, LoRaRadioState::Listening, false, 3001);
    CHECK(!model.sendRejectedVisible(3001));
}

TEST_CASE(lora_app_does_not_request_duplicate_send_while_transmitting) {
    LoRaAppModel model;
    auto result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Listening,
                               false, 50);
    CHECK_EQ(result.effect, LoRaAppEffect::RequestTransmit);
    result = model.handle({InputAction::Confirm, '\0'}, LoRaRadioState::Transmitting,
                          false, 51);
    CHECK_EQ(result.effect, LoRaAppEffect::None);
    CHECK(model.sendRejectedVisible(84));
}

TEST_CASE(media_library_filters_sorts_and_exposes_display_names) {
    MediaLibrary library;
    CHECK(!library.add("/Music/not-a-track.wav", 10));
    CHECK(library.addDirectory("/Music/周杰伦"));
    CHECK(library.add("/Music/zebra.MP3", 300));
    CHECK(library.add("/Music/Alpha.mp3", 100));
    CHECK(library.add("/Music/beta.mP3", 200));
    library.sort();

    CHECK_EQ(library.size(), 4u);
    CHECK(library.at(0).directory);
    CHECK_STR_EQ(mediaTrackName(library.at(0)), "周杰伦");
    CHECK_STR_EQ(mediaTrackName(library.at(1)), "Alpha.mp3");
    CHECK_STR_EQ(mediaTrackName(library.at(2)), "beta.mP3");
    CHECK_STR_EQ(mediaTrackName(library.at(3)), "zebra.MP3");
    CHECK_EQ(library.at(2).bytes, 200u);
    CHECK(library.hasPlayableTrack());
    CHECK(mediaPathIsMp3("song.mp3"));
    CHECK(!mediaPathIsMp3(".mp3"));
    CHECK(!mediaPathIsMp3("song.mp30"));
}

TEST_CASE(media_library_is_bounded_and_selection_wraps) {
    MediaLibrary library;
    for (std::size_t index = 0; index < kMediaTrackCapacity + 1; ++index) {
        char path[40];
        std::snprintf(path, sizeof(path), "/Music/%02u.mp3", static_cast<unsigned>(index));
        const bool added = library.add(path, static_cast<uint32_t>(index));
        CHECK_EQ(added, index < kMediaTrackCapacity);
    }
    CHECK_EQ(library.size(), kMediaTrackCapacity);
    CHECK(library.truncated());
    CHECK(library.add("/Music/00a.mp3", 99));
    library.sort();
    CHECK_STR_EQ(mediaTrackName(library.at(1)), "00a.mp3");
    CHECK_STR_EQ(mediaTrackName(library.at(kMediaTrackCapacity - 1)), "62.mp3");
    library.moveSelection(-1);
    CHECK_EQ(library.selectedIndex(), kMediaTrackCapacity - 1);
    library.moveSelection(1);
    CHECK_EQ(library.selectedIndex(), 0u);
    library.select(4);
    CHECK_EQ(library.selectedIndex(), 4u);
    library.select(kMediaTrackCapacity);
    CHECK_EQ(library.selectedIndex(), 4u);

    char parent[kMediaPathCapacity]{};
    CHECK(mediaParentPath("/Music/周杰伦/七里香", parent, sizeof(parent)));
    CHECK_STR_EQ(parent, "/Music/周杰伦");
    CHECK(mediaParentPath(parent, parent, sizeof(parent)));
    CHECK_STR_EQ(parent, "/Music");
    CHECK(!mediaParentPath("/", parent, sizeof(parent)));
}

TEST_CASE(media_progress_and_elapsed_clock_are_bounded_and_wrap_safe) {
    CHECK_EQ(mediaProgressPercent(0, 0), 0);
    CHECK_EQ(mediaProgressPercent(50, 200), 25);
    CHECK_EQ(mediaProgressPercent(300, 200), 100);

    MediaElapsedClock clock;
    clock.start(UINT32_MAX - 10u);
    CHECK_EQ(clock.elapsed(4), 15u);
    clock.pause(9);
    CHECK_EQ(clock.elapsed(100), 20u);
    clock.resume(100);
    CHECK_EQ(clock.elapsed(110), 30u);
    clock.stop();
    CHECK_EQ(clock.elapsed(999), 0u);
}

TEST_CASE(media_app_maps_library_playback_volume_rescan_and_home) {
    MediaAppModel model;
    MediaAppInputState fileState{true, true, false, false};
    MediaAppInputState directoryState{true, false, true, false};
    MediaAppInputState rootState{true, true, false, true};
    MediaAppInputState emptyState{};
    CHECK_EQ(model.enter(), MediaAppEffect::Scan);
    CHECK_EQ(model.handle({InputAction::Up, '\0'}, fileState).effect,
             MediaAppEffect::SelectPrevious);
    CHECK_EQ(model.handle({InputAction::Down, '\0'}, fileState).effect,
             MediaAppEffect::SelectNext);
    CHECK_EQ(model.handle({InputAction::Left, '\0'}, fileState).effect,
             MediaAppEffect::PlayPrevious);
    CHECK_EQ(model.handle({InputAction::Right, '\0'}, fileState).effect,
             MediaAppEffect::PlayNext);
    CHECK_EQ(model.handle({InputAction::Confirm, '\0'}, fileState).effect,
             MediaAppEffect::ToggleSelected);
    CHECK_EQ(model.handle({InputAction::Confirm, '\0'}, directoryState).effect,
             MediaAppEffect::OpenSelectedDirectory);
    CHECK_EQ(model.handle({InputAction::Erase, '\0'}, directoryState).effect,
             MediaAppEffect::GoParentDirectory);
    CHECK_EQ(model.handle({InputAction::Back, '\0'}, directoryState).effect,
             MediaAppEffect::GoParentDirectory);
    CHECK_EQ(model.handle({InputAction::Tab, '\0'}, emptyState).effect, MediaAppEffect::Scan);
    CHECK_EQ(model.handle({InputAction::None, '-'}, emptyState).volumeDelta, -5);
    CHECK_EQ(model.handle({InputAction::None, '+'}, emptyState).volumeDelta, 5);
    CHECK_EQ(model.handle({InputAction::Erase, '\0'}, rootState).effect,
             MediaAppEffect::StopAndGoHome);
    CHECK_EQ(model.handle({InputAction::Confirm, '\0'}, emptyState).effect,
             MediaAppEffect::None);
}

TEST_CASE(media_volume_request_is_explicit_and_consumed_once) {
    SystemContext context;
    context.requestVolumeDelta(-5);
    CHECK_EQ(context.takeRequestedCommand(), SystemCommand::AdjustVolume);
    int8_t delta = 0;
    CHECK(context.takeVolumeDelta(delta));
    CHECK_EQ(delta, -5);
    CHECK(!context.takeVolumeDelta(delta));
}

TEST_CASE(motion_level_uses_roll_pitch_formula_and_alpha_filter) {
    MotionClassifier classifier;
    classifier.update(MotionSample{0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, 0);
    CHECK(std::fabs(classifier.level().rollDegrees - 90.0f) < 0.01f);
    CHECK(std::fabs(classifier.level().pitchDegrees - 0.0f) < 0.01f);

    classifier.update(MotionSample{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, 1);
    CHECK(std::fabs(classifier.level().rollDegrees - 72.0f) < 0.01f);
    CHECK(std::fabs(classifier.level().pitchDegrees + 18.0f) < 0.01f);
}

TEST_CASE(motion_exposes_acceleration_and_gyro_magnitudes) {
    MotionClassifier classifier;
    classifier.update(MotionSample{0.0f, 0.0f, 1.3f, 3.0f, 4.0f, 12.0f}, 0);
    CHECK(std::fabs(classifier.accelerationMagnitude() - 1.3f) < 0.01f);
    CHECK(std::fabs(classifier.accelerationDeviation() - 0.3f) < 0.01f);
    CHECK(std::fabs(classifier.gyroMagnitude() - 13.0f) < 0.01f);
}

TEST_CASE(motion_still_thresholds_are_strict) {
    MotionClassifier belowAccelThreshold;
    for (uint32_t now = 0; now < 5; ++now) {
        belowAccelThreshold.update(MotionSample{0.0f, 0.0f, 1.079f, 0.0f, 0.0f, 0.0f}, now);
    }
    CHECK_EQ(belowAccelThreshold.activity(), MotionActivity::Still);

    MotionClassifier accelBoundary;
    for (uint32_t now = 0; now < 5; ++now) {
        accelBoundary.update(MotionSample{0.0f, 0.0f, 1.08f, 0.0f, 0.0f, 0.0f}, now);
    }
    CHECK_EQ(accelBoundary.activity(), MotionActivity::Moving);

    MotionClassifier gyroBoundary;
    for (uint32_t now = 0; now < 5; ++now) {
        gyroBoundary.update(MotionSample{0.0f, 0.0f, 1.0f, 10.0f, 0.0f, 0.0f}, now);
    }
    CHECK_EQ(gyroBoundary.activity(), MotionActivity::Moving);
}

TEST_CASE(motion_shake_thresholds_are_inclusive) {
    MotionClassifier accelBoundary;
    accelBoundary.update(MotionSample{0.0f, 0.0f, 1.45f, 0.0f, 0.0f, 0.0f}, 0);
    CHECK_EQ(accelBoundary.activity(), MotionActivity::Shake);

    MotionClassifier gyroBoundary;
    gyroBoundary.update(MotionSample{0.0f, 0.0f, 1.0f, 180.0f, 0.0f, 0.0f}, 0);
    CHECK_EQ(gyroBoundary.activity(), MotionActivity::Shake);
}

TEST_CASE(motion_requires_five_consecutive_non_shake_candidates) {
    MotionClassifier classifier;
    const MotionSample still{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    const MotionSample moving{0.0f, 0.0f, 1.1f, 0.0f, 0.0f, 0.0f};

    for (uint32_t now = 0; now < 4; ++now) classifier.update(still, now);
    CHECK_EQ(classifier.activity(), MotionActivity::Moving);
    classifier.update(still, 4);
    CHECK_EQ(classifier.activity(), MotionActivity::Still);

    for (uint32_t now = 5; now < 9; ++now) classifier.update(moving, now);
    CHECK_EQ(classifier.activity(), MotionActivity::Still);
    classifier.update(moving, 9);
    CHECK_EQ(classifier.activity(), MotionActivity::Moving);
}

TEST_CASE(motion_hysteresis_resets_after_candidate_interruption) {
    MotionClassifier classifier;
    const MotionSample still{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    const MotionSample moving{0.0f, 0.0f, 1.1f, 0.0f, 0.0f, 0.0f};

    for (uint32_t now = 0; now < 4; ++now) classifier.update(still, now);
    CHECK_EQ(classifier.activity(), MotionActivity::Moving);
    classifier.update(moving, 4);
    CHECK_EQ(classifier.activity(), MotionActivity::Moving);

    for (uint32_t now = 5; now < 9; ++now) classifier.update(still, now);
    CHECK_EQ(classifier.activity(), MotionActivity::Moving);
    classifier.update(still, 9);
    CHECK_EQ(classifier.activity(), MotionActivity::Still);
}

TEST_CASE(motion_shake_latch_is_wrap_safe_and_restarts_hysteresis_after_500_ms) {
    MotionClassifier classifier;
    const MotionSample shake{0.0f, 0.0f, 1.0f, 180.0f, 0.0f, 0.0f};
    const MotionSample still{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    const MotionSample latchSample{0.0f, 0.0f, 1.25f, 3.0f, 4.0f, 12.0f};
    constexpr uint32_t start = 0xFFFFFF00u;

    classifier.update(shake, start);
    CHECK_EQ(classifier.activity(), MotionActivity::Shake);
    classifier.update(latchSample, start + 250);
    CHECK_EQ(classifier.activity(), MotionActivity::Shake);
    CHECK(std::fabs(classifier.accelerationMagnitude() - 1.25f) < 0.01f);
    CHECK(std::fabs(classifier.accelerationDeviation() - 0.25f) < 0.01f);
    CHECK(std::fabs(classifier.gyroMagnitude() - 13.0f) < 0.01f);
    CHECK(std::fabs(classifier.peakAccelerationDeviation() - 0.25f) < 0.01f);
    for (uint32_t elapsed = 496; elapsed < 500; ++elapsed) {
        classifier.update(still, start + elapsed);
    }
    CHECK_EQ(classifier.activity(), MotionActivity::Shake);

    for (uint32_t elapsed = 500; elapsed < 504; ++elapsed) {
        classifier.update(still, start + elapsed);
    }
    CHECK_EQ(classifier.activity(), MotionActivity::Shake);
    classifier.update(still, start + 504);
    CHECK_EQ(classifier.activity(), MotionActivity::Still);
}

TEST_CASE(motion_peak_tracks_maximum_and_reset_uses_current_sample) {
    MotionClassifier classifier;
    classifier.update(MotionSample{0.0f, 0.0f, 1.6f, 0.0f, 0.0f, 0.0f}, 0);
    classifier.update(MotionSample{0.0f, 0.0f, 1.1f, 0.0f, 0.0f, 0.0f}, 1);
    CHECK(std::fabs(classifier.peakAccelerationDeviation() - 0.6f) < 0.01f);
    classifier.resetPeak();
    CHECK(std::fabs(classifier.peakAccelerationDeviation() - 0.1f) < 0.01f);

    MotionClassifier emptyClassifier;
    emptyClassifier.resetPeak();
    CHECK_EQ(emptyClassifier.peakAccelerationDeviation(), 0.0f);
}

int main() {
    app_resource_profiles_isolate_foreground_workloads();
    weather_and_media_profiles_have_explicit_tradeoffs();
    ssh_error_detail_redacts_endpoint_identity();
    ssh_error_detail_stays_single_line_and_never_partially_copies_secret();
    ssh_retry_waits_after_failure_not_after_attempt_start();
    ssh_retry_is_cancelable_and_wrap_safe();
    ssh_transport_uses_the_low_memory_cipher_in_both_directions();
    plain_a();
    mac_modifiers();
    fn_navigation_and_escape();
    fn_function_and_editing_keys();
    punctuation_remains_available_without_fn();
    all_printable_keys_use_us_hid_usages();
    more_than_six_keys_reports_rollover();
    system_fn_navigation_is_local_and_edge_triggered();
    system_confirm_back_and_tab_are_local();
    text_keymap_covers_wifi_password_characters();
    text_mode_is_local_and_uses_explicit_edit_actions();
    text_mode_uses_fn_navigation_to_move_between_fields();
    text_mode_exposes_tab_for_multi_field_editors();
    terminal_mode_emits_local_printable_characters();
    terminal_mode_converts_ctrl_letters_to_control_bytes();
    terminal_mode_exposes_navigation_and_terminal_controls();
    terminal_mode_reserves_option_navigation_for_local_scrollback();
    terminal_input_encodes_shell_control_sequences();
    keyboard_input_is_dropped_when_disconnected();
    keyboard_input_becomes_hid_when_connected();
    enter_used_to_open_keyboard_does_not_leak_to_mac();
    reconnect_waits_for_held_keys_to_release();
    g0_long_suppresses_short();
    g0_short_fires_on_release();
    settings_defaults_are_valid();
    settings_version_mismatch_returns_defaults();
    settings_sanitizer_clamps_and_repairs();
    localization_switches_without_dynamic_storage();
    launcher_starts_on_keyboard_and_wraps();
    launcher_confirm_requests_selected_app();
    wifi_and_weather_labels_cover_visible_states();
    weather_localized_text_covers_conditions_states_and_errors();
    keyboard_localized_text_covers_every_state_and_error();
    ssh_localized_text_covers_every_state_and_error();
    lora_localized_text_covers_every_radio_state();
    media_localized_text_covers_states_known_details_and_passthrough();
    settings_localized_text_covers_reset_and_storage_failures();
    wifi_recovery_tries_the_existing_link_before_scanning();
    wifi_recovery_scan_backoff_is_bounded_and_wrap_safe();
    wifi_recovery_success_cancels_pending_work();
    wifi_profiles_keep_eight_recent_networks_without_exposing_passwords();
    wifi_profiles_reject_empty_or_unterminated_credentials();
    ssh_hosts_keep_six_recent_endpoints();
    ssh_hosts_reject_blank_or_unterminated_fields();
    ssh_hosts_support_edit_delete_and_recent_order();
    ssh_host_record_round_trips_and_rejects_corruption();
    ssh_runtime_budget_fits_observed_cardputer_heap();
    display_back_buffer_preserves_ssh_authentication_headroom();
    indexed_display_palette_has_no_conflicting_color_indices();
    terminal_buffer_writes_text_and_tracks_crlf_cursor();
    terminal_buffer_scrolls_and_exposes_local_history();
    terminal_buffer_handles_ansi_cursor_motion_and_line_erase();
    terminal_buffer_handles_ansi_clear_and_absolute_position();
    terminal_buffer_preserves_basic_ansi_colors_per_cell();
    terminal_buffer_handles_relative_cursor_motion_across_writes();
    terminal_buffer_handles_backspace_and_tab_stops();
    terminal_buffer_ignores_shell_title_and_charset_sequences();
    ssh_app_host_list_navigates_and_emits_explicit_actions();
    ssh_app_editor_builds_a_valid_host_without_heap_input();
    ssh_app_editor_can_load_and_update_an_existing_host();
    ssh_app_editor_stays_open_and_marks_invalid_records();
    ssh_app_session_pages_support_cancel_reconnect_and_quick_commands();
    ssh_app_requires_confirmation_before_deleting_a_host();
    local_clock_rejects_unsynced_time_and_applies_utc_offset();
    weather_display_keeps_successful_data_when_inputs_disappear();
    gps_state_distinguishes_stream_search_fix_and_stale();
    gps_page_navigation_wraps_across_all_four_pages();
    gps_compass_points_cover_cardinal_and_intercardinal_directions();
    gps_fix_quality_and_mode_have_readable_labels();
    gps_localized_text_covers_states_compass_quality_and_mode();
    single_host_policy_allows_pairing_only_without_a_stored_bond();
    ble_advertising_deadline_handles_millis_wraparound();
    ble_report_gate_sends_release_then_deduplicates();
    disconnect_blocks_reports_and_resets_gate();
    diagnostics_ring_is_bounded_and_newest_first();
    diagnostics_messages_are_safely_truncated();
    diagnostics_sink_replays_history_and_keeps_full_messages();
    async_diagnostics_are_drained_into_the_persistent_sink();
    serial_log_commands_are_normalized_and_require_clear_confirmation();
    quick_settings_adjusts_values_and_clamps();
    quick_settings_close_reports_whether_persistence_is_needed();
    settings_categories_open_and_back_out();
    settings_destructive_actions_require_confirmation();
    settings_bluetooth_controls_emit_explicit_effects();
    settings_wifi_scan_selection_and_forget_are_explicit();
    settings_system_actions_and_diagnostics_are_reachable();
    settings_storage_mount_and_format_are_explicit();
    settings_factory_reset_requires_its_own_confirmation();
    lora_draft_accepts_printable_ascii_and_copies_exact_payload();
    lora_send_requires_a_listening_nonempty_draft_and_records_exact_tx();
    lora_transmitting_draft_is_immutable_until_completion();
    lora_pending_tx_snapshots_same_frame_payload_and_locks_edits();
    lora_tx_watchdog_deadline_is_inclusive_and_wrap_safe();
    lora_history_evicts_oldest_and_has_readable_direction_labels();
    lora_rx_sanitizes_payload_rejects_oversize_and_tracks_quality();
    lora_state_transitions_cover_crc_restart_and_persistent_error();
    lora_app_lifecycle_emits_only_lazy_start_and_draft_clear();
    lora_app_maps_text_edit_send_and_home_actions();
    lora_app_rejected_send_feedback_is_bounded_visible_and_wrap_safe();
    lora_app_does_not_request_duplicate_send_while_transmitting();
    media_library_filters_sorts_and_exposes_display_names();
    media_library_is_bounded_and_selection_wraps();
    media_progress_and_elapsed_clock_are_bounded_and_wrap_safe();
    media_app_maps_library_playback_volume_rescan_and_home();
    media_volume_request_is_explicit_and_consumed_once();
    motion_level_uses_roll_pitch_formula_and_alpha_filter();
    motion_exposes_acceleration_and_gyro_magnitudes();
    motion_still_thresholds_are_strict();
    motion_shake_thresholds_are_inclusive();
    motion_requires_five_consecutive_non_shake_candidates();
    motion_hysteresis_resets_after_candidate_interruption();
    motion_shake_latch_is_wrap_safe_and_restarts_hysteresis_after_500_ms();
    motion_peak_tracks_maximum_and_reset_uses_current_sample();
    return pd_test::finish();
}
