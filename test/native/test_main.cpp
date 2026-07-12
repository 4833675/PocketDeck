#include "test_harness.h"

#include <array>
#include <utility>

#include "core/g0_gesture.h"
#include "core/input_router.h"
#include "core/mac_keymap.h"
#include "core/system_settings.h"

using namespace pd;

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
    settings.deviceName.fill('\0');
    settings.hostLabel.fill('X');
    const auto fixed = sanitizeSettings(settings);
    CHECK_EQ(fixed.brightness, 100);
    CHECK_EQ(fixed.volume, 100);
    CHECK_EQ(fixed.sleepSeconds, 15);
    CHECK_STR_EQ(fixed.deviceName.data(), "Pocket Deck");
    CHECK_STR_EQ(fixed.hostLabel.data(), "Mac");
}

int main() {
    plain_a();
    mac_modifiers();
    fn_navigation_and_escape();
    fn_function_and_editing_keys();
    punctuation_remains_available_without_fn();
    all_printable_keys_use_us_hid_usages();
    more_than_six_keys_reports_rollover();
    system_fn_navigation_is_local_and_edge_triggered();
    system_confirm_back_and_tab_are_local();
    keyboard_input_is_dropped_when_disconnected();
    keyboard_input_becomes_hid_when_connected();
    enter_used_to_open_keyboard_does_not_leak_to_mac();
    reconnect_waits_for_held_keys_to_release();
    g0_long_suppresses_short();
    g0_short_fires_on_release();
    settings_defaults_are_valid();
    settings_version_mismatch_returns_defaults();
    settings_sanitizer_clamps_and_repairs();
    return pd_test::finish();
}
