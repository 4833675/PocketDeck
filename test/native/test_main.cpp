#include "test_harness.h"

#include <array>
#include <utility>

#include "apps/launcher/launcher_model.h"
#include "apps/settings/settings_model.h"
#include "core/ble_keyboard_policy.h"
#include "core/clock_data.h"
#include "core/g0_gesture.h"
#include "core/gps_data.h"
#include "core/input_router.h"
#include "core/mac_keymap.h"
#include "core/system_settings.h"
#include "core/text_keymap.h"
#include "core/weather_data.h"
#include "core/wifi_data.h"
#include "services/diagnostics_service.h"
#include "ui/quick_settings_model.h"

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

TEST_CASE(launcher_starts_on_keyboard_and_wraps) {
    LauncherModel model;
    CHECK_EQ(model.selected(), AppId::Keyboard);
    model.handle(InputAction::Right);
    CHECK_EQ(model.selected(), AppId::Gps);
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
    CHECK_EQ(model.handle(InputAction::Confirm), AppId::Gps);
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
    CHECK_STR_EQ(weatherStateLabel(WeatherState::Fetching), "FETCHING");
    CHECK_STR_EQ(weatherCodeLabel(0), "CLEAR");
    CHECK_STR_EQ(weatherCodeLabel(61), "RAIN");
    CHECK_STR_EQ(weatherCodeLabel(95), "THUNDERSTORM");
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
    model.handle(InputAction::Down);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(model.page(), SettingsPage::ConfirmForgetWifi);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::ForgetWifi);
}

TEST_CASE(settings_system_actions_and_diagnostics_are_reachable) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::Diagnostics);
    model.handle(InputAction::Back);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    CHECK_EQ(model.page(), SettingsPage::ConfirmRestart);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::Restart);
}

TEST_CASE(settings_factory_reset_requires_its_own_confirmation) {
    SettingsModel model;
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    model.handle(InputAction::Confirm);
    model.handle(InputAction::Down);
    model.handle(InputAction::Down);
    auto result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::None);
    CHECK_EQ(model.page(), SettingsPage::ConfirmFactoryReset);
    result = model.handle(InputAction::Confirm);
    CHECK_EQ(result.effect, SettingsEffect::FactoryReset);
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
    text_keymap_covers_wifi_password_characters();
    text_mode_is_local_and_uses_explicit_edit_actions();
    keyboard_input_is_dropped_when_disconnected();
    keyboard_input_becomes_hid_when_connected();
    enter_used_to_open_keyboard_does_not_leak_to_mac();
    reconnect_waits_for_held_keys_to_release();
    g0_long_suppresses_short();
    g0_short_fires_on_release();
    settings_defaults_are_valid();
    settings_version_mismatch_returns_defaults();
    settings_sanitizer_clamps_and_repairs();
    launcher_starts_on_keyboard_and_wraps();
    launcher_confirm_requests_selected_app();
    wifi_and_weather_labels_cover_visible_states();
    local_clock_rejects_unsynced_time_and_applies_utc_offset();
    weather_display_keeps_successful_data_when_inputs_disappear();
    gps_state_distinguishes_stream_search_fix_and_stale();
    gps_compass_points_cover_cardinal_and_intercardinal_directions();
    gps_fix_quality_and_mode_have_readable_labels();
    single_host_policy_allows_pairing_only_without_a_stored_bond();
    ble_advertising_deadline_handles_millis_wraparound();
    ble_report_gate_sends_release_then_deduplicates();
    disconnect_blocks_reports_and_resets_gate();
    diagnostics_ring_is_bounded_and_newest_first();
    diagnostics_messages_are_safely_truncated();
    quick_settings_adjusts_values_and_clamps();
    quick_settings_close_reports_whether_persistence_is_needed();
    settings_categories_open_and_back_out();
    settings_destructive_actions_require_confirmation();
    settings_bluetooth_controls_emit_explicit_effects();
    settings_wifi_scan_selection_and_forget_are_explicit();
    settings_system_actions_and_diagnostics_are_reachable();
    settings_factory_reset_requires_its_own_confirmation();
    return pd_test::finish();
}
