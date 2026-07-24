# Pocket Deck Foundation and BLE Keyboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a clean, flashable Pocket Deck foundation with the Graphite Mint launcher, global G0 navigation, macOS key mapping, one-host secure BLE HID keyboard, and a minimal Bluetooth/System settings surface.

**Architecture:** Pure C++ core modules translate physical keys, route input, validate settings, and recognize G0 gestures; they are host-tested without Arduino headers. Thin Cardputer drivers feed those modules. A single `System` owns long-lived services and static app instances, so foreground app changes never reconstruct BLE.

**Tech Stack:** PlatformIO 6.1.19, `platformio/espressif32` 7.0.1, Arduino-ESP32 2.0.17, M5Cardputer 1.1.1, M5Unified 0.2.17, framework-native Bluedroid `BLEHIDDevice`, Apple clang host tests.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck`; do not modify either Claude Buddy repository.
- Target only M5Stack Cardputer Adv / ESP32-S3FN8 with 8 MB flash and no PSRAM.
- Do not define `BOARD_HAS_PSRAM` and do not allocate from PSRAM.
- Use C++17 and keep Arduino/M5/BLE headers out of `src/core/`.
- BLE has one stable identity, one HID profile, and one bonded macOS host.
- G0 short press returns Home; G0 long press at 600 ms opens Quick Settings and suppresses the short action.
- Typed content is never rendered, logged, or persisted.
- Disconnected HID input is dropped and never replayed.
- UI is English, 240×135 landscape, Graphite Mint.
- This plan deliberately stops before Wi-Fi and the full Settings categories; those form the next independently testable plan after BLE hardware validation.

---

## File map

```text
.gitignore                                  generated files only
platformio.ini                              Cardputer Adv build configuration
partitions_8mb.csv                          no-OTA 8 MB flash layout
include/pocket_deck_config.h                version, dimensions, timing constants
src/main.cpp                                Arduino setup/loop delegation only
src/core/physical_key.h                     physical 4×14 key identities
src/core/key_state.h                        fixed-capacity pressed-key snapshot
src/core/hid_report.h                       standard 8-byte boot report value type
src/core/mac_keymap.h/.cpp                  physical key → macOS HID mapping
src/core/input.h                             logical system input types
src/core/input_router.h/.cpp                system/HID routing and edge detection
src/core/g0_gesture.h/.cpp                  debounced short/long recognition
src/core/system_settings.h/.cpp              defaults and validation
src/core/app.h                               app lifecycle interface
src/core/system_context.h                    service references exposed to apps
src/core/system.h/.cpp                       boot, foreground app, event loop
src/drivers/board.h/.cpp                     M5Cardputer init/update and key scan
src/drivers/display.h/.cpp                   one 240×135 M5Canvas and push
src/services/ble_keyboard_service.h/.cpp     Bluedroid HID, bond, security, reports
src/services/diagnostics_service.h/.cpp      bounded event log and runtime snapshot
src/services/settings_store.h/.cpp           Preferences adapter for validated POD
src/ui/theme.h                               Graphite Mint token constants
src/ui/status_bar.h/.cpp                     shared top status strip
src/ui/quick_settings.h/.cpp                 G0-long overlay
src/apps/launcher/launcher_app.h/.cpp         horizontal Keyboard/Settings cards
src/apps/keyboard/keyboard_app.h/.cpp         connection/passkey/modifier screen
src/apps/settings/settings_app.h/.cpp         Bluetooth and System sections only
test/native/test_harness.h                   dependency-free assertions
test/native/test_main.cpp                    pure core test executable
scripts/test-native.sh                       deterministic host compile/run command
README.md                                    build, flash, pairing, controls
```

### Task 1: Bootstrap a clean, buildable Cardputer Adv project

**Files:**
- Create: `.gitignore`
- Create: `platformio.ini`
- Create: `partitions_8mb.csv`
- Create: `include/pocket_deck_config.h`
- Create: `src/main.cpp`
- Create: `README.md`

**Interfaces:**
- Produces: a `cardputer-adv` PlatformIO environment and `pd::config` constants consumed by all later tasks.

- [ ] **Step 1: Add generated-file exclusions**

```gitignore
.pio/
.build/
.DS_Store
*.bin
*.elf
```

- [ ] **Step 2: Add the exact build configuration**

```ini
[platformio]
default_envs = cardputer-adv

[env:cardputer-adv]
platform = espressif32@7.0.1
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
board_build.partitions = partitions_8mb.csv
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -DCORE_DEBUG_LEVEL=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    m5stack/M5Unified @ 0.2.17
    m5stack/M5Cardputer @ 1.1.1
```

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  factory, 0x10000,  0x300000,
spiffs,   data, spiffs,  0x310000, 0x4F0000,
```

- [ ] **Step 3: Add immutable project constants**

```cpp
#pragma once
#include <cstdint>

namespace pd::config {
inline constexpr char kProductName[] = "Pocket Deck";
inline constexpr char kFirmwareVersion[] = "0.1.0";
inline constexpr int16_t kScreenWidth = 240;
inline constexpr int16_t kScreenHeight = 135;
inline constexpr uint32_t kSerialBaud = 115200;
inline constexpr uint32_t kG0LongPressMs = 600;
inline constexpr uint8_t kDefaultBrightness = 78;
inline constexpr uint8_t kDefaultVolume = 55;
}
```

- [ ] **Step 4: Add a temporary hardware smoke entry point**

```cpp
#include <Arduino.h>
#include <M5Cardputer.h>
#include "pocket_deck_config.h"

void setup() {
    Serial.begin(pd::config::kSerialBaud);
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.drawString(pd::config::kProductName, 120, 67);
    Serial.println("Pocket Deck bootstrap");
}

void loop() {
    M5Cardputer.update();
    delay(5);
}
```

- [ ] **Step 5: Add the initial README**

````markdown
# Pocket Deck

Pocket Deck is a standalone, keyboard-first system for the M5Stack Cardputer Adv.
It is a new project and does not depend on Claude Desktop or Claude Buddy firmware.

## Current implementation phase

Foundation and secure single-host BLE keyboard for macOS.

## Build

```bash
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

The firmware targets Cardputer Adv only and does not use PSRAM.
````

- [ ] **Step 6: Build the bootstrap firmware**

Run: `pio run -e cardputer-adv`

Expected: exit 0, `firmware.bin` produced, no PSRAM compile/link dependency.

- [ ] **Step 7: Commit the bootstrap**

```bash
git add .gitignore platformio.ini partitions_8mb.csv include/pocket_deck_config.h src/main.cpp README.md
git commit -m "build: bootstrap Pocket Deck firmware"
```

### Task 2: Build and test the macOS HID keymap

**Files:**
- Create: `src/core/physical_key.h`
- Create: `src/core/key_state.h`
- Create: `src/core/hid_report.h`
- Create: `src/core/mac_keymap.h`
- Create: `src/core/mac_keymap.cpp`
- Create: `test/native/test_harness.h`
- Create: `test/native/test_main.cpp`
- Create: `scripts/test-native.sh`

**Interfaces:**
- Produces: `pd::PhysicalKey`, `pd::KeyState`, `pd::HidReport`, and `pd::MacKeymap::buildReport(const KeyState&)`.

- [ ] **Step 1: Write failing keymap tests**

The test file must assert these exact cases:

```cpp
TEST_CASE(plain_a) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::A}),
             HidReport::single(0x00, 0x04));
}

TEST_CASE(mac_modifiers) {
    KeyState keys{PhysicalKey::Ctrl, PhysicalKey::Opt, PhysicalKey::Alt, PhysicalKey::A};
    CHECK_EQ(MacKeymap::buildReport(keys), HidReport::single(0x0D, 0x04));
}

TEST_CASE(fn_arrows_and_escape) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Semicolon}),
             HidReport::single(0x00, 0x52));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Backtick}),
             HidReport::single(0x00, 0x29));
}

TEST_CASE(fn_function_keys) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Num1}),
             HidReport::single(0x00, 0x3A));
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn, PhysicalKey::Equal}),
             HidReport::single(0x00, 0x45));
}

TEST_CASE(fn_is_never_transmitted) {
    CHECK_EQ(MacKeymap::buildReport(KeyState{PhysicalKey::Fn}), HidReport{});
}
```

- [ ] **Step 2: Add and run the native compile script to prove RED**

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build
sources=(test/native/test_main.cpp src/core/mac_keymap.cpp)
for source in src/core/input_router.cpp src/core/g0_gesture.cpp src/core/system_settings.cpp; do
  if [[ -f "$source" ]]; then
    sources+=("$source")
  fi
done
c++ -std=c++17 -Wall -Wextra -Werror -Isrc -Itest/native \
  "${sources[@]}" -o .build/native_tests
.build/native_tests
```

Run: `chmod +x scripts/test-native.sh && scripts/test-native.sh`

Expected: compile failure because the keymap types do not exist yet.

- [ ] **Step 3: Implement fixed-capacity key and report value types**

`PhysicalKey` enumerates all 56 matrix positions in row-major order, with names matching the printed key. `KeyState` owns `std::array<PhysicalKey, 8>` plus `count`, has an initializer-list constructor, `contains()`, and never allocates. `HidReport` owns `modifier`, `reserved`, and six key usages; provide `bytes()`, equality, `empty()`, and `single(modifier, usage)`.

- [ ] **Step 4: Implement the US/macOS mapping**

`MacKeymap::buildReport()` must:

```cpp
HidReport MacKeymap::buildReport(const KeyState& state) {
    HidReport report{};
    const bool fn = state.contains(PhysicalKey::Fn);
    for (uint8_t i = 0; i < state.count; ++i) {
        const PhysicalKey key = state.keys[i];
        if (key == PhysicalKey::Fn) continue;
        if (key == PhysicalKey::Ctrl) { report.modifier |= 0x01; continue; }
        if (key == PhysicalKey::Shift) { report.modifier |= 0x02; continue; }
        if (key == PhysicalKey::Opt) { report.modifier |= 0x04; continue; }
        if (key == PhysicalKey::Alt) { report.modifier |= 0x08; continue; }
        const uint8_t usage = fn ? fnUsage(key) : normalUsage(key);
        report.push(usage);
    }
    return report;
}
```

The lookup tables must implement every printable US key plus the approved Fn table. Zero usages are skipped; more than six non-modifier keys set all six slots to `0x01` (HID ErrorRollOver).

- [ ] **Step 5: Run tests to prove GREEN**

Run: `scripts/test-native.sh`

Expected: all keymap cases print `PASS`; process exits 0.

- [ ] **Step 6: Commit the keymap**

```bash
git add src/core test/native scripts/test-native.sh
git commit -m "feat: add tested macOS keymap"
```

### Task 3: Test and implement system input routing and G0 gestures

**Files:**
- Create: `src/core/input.h`
- Create: `src/core/input_router.h`
- Create: `src/core/input_router.cpp`
- Create: `src/core/g0_gesture.h`
- Create: `src/core/g0_gesture.cpp`
- Modify: `test/native/test_main.cpp`

**Interfaces:**
- Consumes: `KeyState`, `HidReport`, `MacKeymap::buildReport()`.
- Produces: `InputRouter::update(const KeyState&, InputMode, bool) -> InputFrame` and `G0Gesture::update(bool, uint32_t) -> G0Action`.

- [ ] **Step 1: Add failing routing and gesture tests**

```cpp
TEST_CASE(system_fn_navigation_is_local) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::Fn, PhysicalKey::Comma}, InputMode::System, false);
    CHECK_EQ(frame.eventCount, 1);
    CHECK_EQ(frame.events[0].action, InputAction::Left);
    CHECK(!frame.hasHidReport);
}

TEST_CASE(keyboard_input_is_dropped_when_disconnected) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, false);
    CHECK_EQ(frame.eventCount, 0);
    CHECK(!frame.hasHidReport);
}

TEST_CASE(keyboard_input_becomes_hid_when_connected) {
    InputRouter router;
    auto frame = router.update(KeyState{PhysicalKey::A}, InputMode::Keyboard, true);
    CHECK(frame.hasHidReport);
    CHECK_EQ(frame.hidReport, HidReport::single(0, 0x04));
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
```

- [ ] **Step 2: Run the tests to prove RED**

Run: `scripts/test-native.sh`

Expected: compile failure for missing input router and G0 types.

- [ ] **Step 3: Implement fixed-capacity `InputFrame` and rising-edge routing**

`InputFrame` contains up to eight `InputEvent` values, `eventCount`, `hasHidReport`, and `hidReport`. In System mode, emit events only for newly pressed keys: Fn+`; , . /`, Enter, Backspace, and Tab. In Keyboard mode, emit no logical events and attach a complete HID report only when connected. Preserve previous key state so held system keys do not repeat at firmware loop speed.

- [ ] **Step 4: Implement debounced G0 semantics**

`G0Gesture` must ignore transitions shorter than 25 ms, emit QuickSettings once at 600 ms, and emit Home only on a debounced release when no long action fired.

- [ ] **Step 5: Run tests to prove GREEN**

Run: `scripts/test-native.sh`

Expected: keymap, routing, and G0 tests all pass.

- [ ] **Step 6: Commit input routing**

```bash
git add src/core/input.h src/core/input_router.* src/core/g0_gesture.* test/native/test_main.cpp
git commit -m "feat: route system and HID input"
```

### Task 4: Test and implement versioned settings defaults

**Files:**
- Create: `src/core/system_settings.h`
- Create: `src/core/system_settings.cpp`
- Modify: `test/native/test_main.cpp`

**Interfaces:**
- Produces: `SystemSettings::defaults()`, `sanitizeSettings(SystemSettings)`, fixed `deviceName[25]`, and fixed `hostLabel[25]`.

- [ ] **Step 1: Add failing validation tests**

```cpp
TEST_CASE(settings_defaults_are_valid) {
    auto settings = SystemSettings::defaults();
    CHECK_EQ(settings.version, SystemSettings::kVersion);
    CHECK_EQ(settings.brightness, 78);
    CHECK_EQ(settings.volume, 55);
    CHECK(settings.bleEnabled);
    CHECK_STR_EQ(settings.deviceName.data(), "Pocket Deck");
}

TEST_CASE(settings_version_mismatch_returns_defaults) {
    auto settings = SystemSettings::defaults();
    settings.version = 99;
    settings.brightness = 255;
    auto fixed = sanitizeSettings(settings);
    CHECK_EQ(fixed.version, SystemSettings::kVersion);
    CHECK_EQ(fixed.brightness, 78);
}

TEST_CASE(settings_sanitizer_clamps_and_repairs) {
    auto settings = SystemSettings::defaults();
    settings.brightness = 255;
    settings.deviceName.fill('\0');
    auto fixed = sanitizeSettings(settings);
    CHECK_EQ(fixed.brightness, 100);
    CHECK_STR_EQ(fixed.deviceName.data(), "Pocket Deck");
}
```

- [ ] **Step 2: Run tests to prove RED**

Run: `scripts/test-native.sh`

Expected: compile failure for missing settings types.

- [ ] **Step 3: Implement POD defaults and sanitization**

Use only fixed-width integers, bools, and `std::array<char, 25>`. Clamp brightness/volume to 0–100 and sleep seconds to 15–3600. A version mismatch returns the complete default record. Empty or unterminated names are replaced by defaults.

- [ ] **Step 4: Run tests to prove GREEN**

Run: `scripts/test-native.sh`

Expected: all pure-core tests pass.

- [ ] **Step 5: Commit settings validation**

```bash
git add src/core/system_settings.* test/native/test_main.cpp
git commit -m "feat: validate Pocket Deck settings"
```

### Task 5: Add Cardputer drivers, display primitives, and app lifecycle

**Files:**
- Create: `src/drivers/board.h/.cpp`
- Create: `src/drivers/display.h/.cpp`
- Create: `src/core/app.h`
- Create: `src/core/system_context.h`
- Create: `src/ui/theme.h`
- Create: `src/ui/status_bar.h/.cpp`
- Create: `src/apps/launcher/launcher_app.h/.cpp`
- Create: `src/core/system.h/.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: pure-core key and routing types.
- Produces: `Board::begin/update/keyState/g0Down/batteryPercent`, `Display::begin/beginFrame/endFrame/canvas`, static `App` lifecycle, and a Home screen.

- [ ] **Step 1: Implement the thin board adapter**

Initialize `M5Cardputer` once, set rotation 1, configure GPIO0 pull-up, and map every `(x,y)` matrix coordinate to the row-major `PhysicalKey` enum. `keyState()` returns at most eight currently pressed keys without heap allocation.

- [ ] **Step 2: Implement the single display canvas**

Create one 16-bit `M5Canvas` at 240×135. `beginFrame()` fills Graphite Mint background; `endFrame()` pushes at `(0,0)`. Define all RGB565 colors in `theme.h`; app code must not contain raw color literals.

- [ ] **Step 3: Implement the app contract and launcher**

Use the approved lifecycle signatures from the design. Launcher holds two static cards (`Keyboard`, `Settings`), starts on Keyboard, uses Left/Right rising-edge actions, and emits an app-open request on Confirm. Render top status, center card, side-card peeks, and bottom hints.

- [ ] **Step 4: Replace the smoke `main.cpp` with delegation**

```cpp
#include "core/system.h"

namespace { pd::System system; }

void setup() { system.begin(); }
void loop() { system.update(); }
```

- [ ] **Step 5: Build firmware**

Run: `pio run -e cardputer-adv`

Expected: exit 0; Home/launcher firmware links without BLE yet.

- [ ] **Step 6: Commit the system shell**

```bash
git add src include
git commit -m "feat: add Pocket Deck system shell"
```

### Task 6: Implement secure single-host BLE HID service

**Files:**
- Create: `src/services/ble_keyboard_service.h`
- Create: `src/services/ble_keyboard_service.cpp`
- Create: `src/services/diagnostics_service.h/.cpp`
- Modify: `src/core/system_context.h`
- Modify: `src/core/system.h/.cpp`

**Interfaces:**
- Consumes: `HidReport` and battery percentage.
- Produces: `BleKeyboardSnapshot`, `begin(name)`, `setEnabled(bool)`, `sendReport(const HidReport&)`, `disconnect()`, `forgetHost()`, and `updateBattery(uint8_t)`.

- [ ] **Step 1: Define a standard report map and immutable snapshot**

The report map has Report ID 1, eight modifier bits, one reserved byte, six 8-bit key usages, and LED output bits. Snapshot states are `Disabled`, `Advertising`, `Pairing`, `Connected`, and `Error`, plus `bonded`, `encrypted`, six-digit `passkey`, and bounded `lastError`.

- [ ] **Step 2: Initialize Bluedroid exactly once**

Call `BLEDevice::init(name)`, create one server, one `BLEHIDDevice`, input report 1, output report 1, manufacturer `Pocket Deck`, PnP data, HID info, battery service, report map, and advertising appearance `HID_KEYBOARD`. Never call `BLEDevice::deinit()` during app switching.

- [ ] **Step 3: Configure secure DisplayOnly pairing**

Generate a random 100000–999999 passkey on entering unpaired pairing state. Configure `ESP_IO_CAP_OUT`, key size 16, init/response ENC+ID keys, and `ESP_LE_AUTH_REQ_SC_MITM_BOND`. Show the passkey through the snapshot; macOS enters it. Authentication callbacks set encrypted/bonded state or an error without blocking.

- [ ] **Step 4: Enforce one host and implement bond removal**

Use `esp_ble_get_bond_device_num()`, `esp_ble_get_bond_device_list()`, and `esp_ble_remove_bond_device()`. If a bond exists, reject/disconnect a peer address not present in the bond list. `forgetHost()` disconnects, removes every bond (v1 permits only one), generates a new passkey, and resumes pairing advertising.

- [ ] **Step 5: Send safe state reports**

`sendReport()` returns immediately when disabled/disconnected. It notifies only when the report differs from the last sent report. On every authenticated connection, notify one empty report before marking typing ready. On disconnect, clear the cached report and restart advertising.

- [ ] **Step 6: Build firmware**

Run: `pio run -e cardputer-adv`

Expected: exit 0 with framework `ESP32 BLE Arduino` dependency; no NimBLE dependency.

- [ ] **Step 7: Commit the BLE service**

```bash
git add src/services src/core/system_context.h src/core/system.*
git commit -m "feat: add secure BLE keyboard service"
```

### Task 7: Add Keyboard app and connect HID routing

**Files:**
- Create: `src/apps/keyboard/keyboard_app.h/.cpp`
- Modify: `src/core/system.h/.cpp`
- Modify: `src/apps/launcher/launcher_app.cpp`

**Interfaces:**
- Consumes: `BleKeyboardSnapshot`, `InputFrame::hidReport`, and System Home requests.
- Produces: a privacy-safe Keyboard screen and complete HID report flow.

- [ ] **Step 1: Register one static Keyboard app**

Entering Keyboard changes `InputMode` to `Keyboard`; exiting sends one empty report and restores `System` mode. It does not start or stop BLE.

- [ ] **Step 2: Route every matrix-state transition**

While Keyboard is foreground, call `InputRouter::update(..., InputMode::Keyboard, snapshot.state == Connected)`. If `hasHidReport`, pass it to `BleKeyboardService::sendReport()`. When disconnected, discard the frame.

- [ ] **Step 3: Render only safe metadata**

Display `PAIRING` plus the six-digit passkey, `ADVERTISING`, `CONNECTED`, `DISCONNECTED`, or `ERROR`; show local host label `Mac`, BLE encryption indicator, battery, and active modifier names. Do not render `KeyState`, HID usages, characters, or prior reports.

- [ ] **Step 4: Build and run native tests**

Run: `scripts/test-native.sh && pio run -e cardputer-adv`

Expected: all host tests pass and firmware links.

- [ ] **Step 5: Commit Keyboard app**

```bash
git add src/apps/keyboard src/apps/launcher src/core/system.*
git commit -m "feat: add Pocket Deck keyboard app"
```

### Task 8: Add minimal Bluetooth/System settings, Quick Settings, and diagnostics

**Files:**
- Create: `src/services/settings_store.h/.cpp`
- Create: `src/apps/settings/settings_app.h/.cpp`
- Create: `src/ui/quick_settings.h/.cpp`
- Modify: `src/core/system.h/.cpp`
- Modify: `src/core/system_context.h`

**Interfaces:**
- Consumes: validated `SystemSettings`, BLE snapshot/actions, diagnostics snapshot, G0 actions.
- Produces: persisted display/BLE preferences, Bluetooth actions, System diagnostics, and the G0-long overlay.

- [ ] **Step 1: Adapt Preferences to the validated POD**

Use namespace `pocketdeck`. Read/write the full versioned blob under key `settings`; sanitize every load. Never store BLE bond keys. Apply brightness and BLE enabled state after load.

- [ ] **Step 2: Implement the initial Settings app**

Expose only two functioning categories in this phase:

- Bluetooth: enabled, state, device name, host label, Disconnect, Forget host & pair new.
- System: version, reset reason, uptime, free heap, minimum heap, recent diagnostics, restart, factory reset.

Factory reset uses a confirmation screen, clears the `pocketdeck` app namespace, removes BLE bonds, and restarts. The later Wi-Fi plan will extend the same operation to erase Wi-Fi credentials. No Wi-Fi or storage menu row appears in this phase.

- [ ] **Step 3: Implement Quick Settings**

Long G0 opens an overlay without leaving the foreground app. Left/Right changes brightness in 10-point steps; Up/Down changes volume in 10-point steps; Enter toggles BLE; Backspace or short G0 closes. Persist changes only when the overlay closes to avoid NVS writes on every key repeat.

- [ ] **Step 4: Implement bounded diagnostics**

Store 12 messages of at most 47 characters in a fixed ring; log reset reason, board init, BLE transitions, authentication result, disconnect, bond deletion, invalid settings fallback, and factory reset. Mirror every event to Serial without secrets or typed data.

- [ ] **Step 5: Run all automated builds**

Run: `scripts/test-native.sh && pio run -e cardputer-adv`

Expected: host tests pass; firmware exits 0; RAM and flash remain within board limits.

- [ ] **Step 6: Commit Settings and diagnostics**

```bash
git add src/services src/apps/settings src/ui src/core
git commit -m "feat: add Bluetooth settings and diagnostics"
```

### Task 9: Document and perform the first hardware acceptance cycle

**Files:**
- Modify: `README.md`
- Create: `docs/validation/ble-keyboard-smoke-test.md`

**Interfaces:**
- Produces: exact user-facing build/pair/test instructions and a durable result log.

- [ ] **Step 1: Document build and flash commands**

README must include:

```bash
scripts/test-native.sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

Document G0 short/long, Fn layer, Mac modifier mapping, pairing passkey direction, Disconnect, and Forget host.

- [ ] **Step 2: Create the smoke-test checklist**

The checklist has explicit pass/fail rows for boot Home, launcher, G0, passkey display, macOS pairing, reboot reconnect, plain keys, punctuation, all four modifiers, Fn arrows, F1/F12, Escape, Caps Lock, Forward Delete, held-key release, disconnect input drop, reconnect all-up, forget/re-pair, and factory reset.

- [ ] **Step 3: Run final automated verification**

Run:

```bash
scripts/test-native.sh
pio run -e cardputer-adv
git diff --check
git status --short
```

Expected: native tests pass; firmware build succeeds; no whitespace errors; only intended documentation changes remain before commit.

- [ ] **Step 4: Commit implementation documentation**

```bash
git add README.md docs/validation/ble-keyboard-smoke-test.md
git commit -m "docs: add Pocket Deck keyboard validation"
```

- [ ] **Step 5: Stop at the hardware boundary**

Do not claim BLE behavior is verified until the user flashes the firmware and returns the checklist results. Record every observed result in `docs/validation/ble-keyboard-smoke-test.md` before beginning the Wi-Fi/full-Settings implementation plan.
