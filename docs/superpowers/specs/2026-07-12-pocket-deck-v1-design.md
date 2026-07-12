# Pocket Deck v1 Design

**Status:** Approved for implementation on 2026-07-12  
**Project root:** `/Users/kx/M5Stack/pocket-deck`  
**Target hardware:** M5Stack Cardputer Adv (Stamp-S3A / ESP32-S3FN8, 8 MB flash, no PSRAM, 240×135 display)

## 1. Product statement

Pocket Deck is a new, standalone, keyboard-first pocket system for the M5Stack Cardputer Adv. It is not a fork of `claude-desktop-buddy`, does not connect to Claude Desktop, and does not copy the old firmware's product logic or source structure. Hardware facts learned during earlier work may be reused, while implementation depends only on official M5Stack/Espressif libraries.

The v1 product is a small system shell with a launcher, settings, Wi-Fi management, and a secure Bluetooth LE keyboard for one macOS host.

## 2. Goals

- Boot into a stable Pocket Deck Home screen with a horizontal app-card launcher.
- Keep system services alive independently from foreground apps.
- Pair once with one Mac and reconnect across device restarts without changing BLE identity.
- Route all Cardputer keys to the Mac only while the Keyboard app is foreground.
- Provide on-device Wi-Fi, Bluetooth, display, sound, power, storage, system, and diagnostics settings.
- Keep the code modular enough that GPS, LoRa, IR, notes, files, and other apps can be added without expanding `main.cpp` into a monolith.
- Preserve `/Users/kx/M5Stack/claude-desktop-buddy-cardputer` unchanged.

## 3. Explicit v1 non-goals

- Claude Desktop integration or the Hardware Buddy protocol.
- Microphone capture, Bluetooth microphone, speech recognition, or dictation.
- Multiple Bluetooth host slots.
- GPS, LoRa, infrared remote, file manager, notes, games, or third-party apps.
- Chinese UI or a bundled CJK font.
- OTA firmware update.
- A dynamic plugin loader or scripting runtime.
- Copying source files from either existing Claude Buddy repository.

## 4. Technology and build constraints

- Build system: PlatformIO.
- Framework: Arduino for ESP32.
- Primary hardware library: official `M5Cardputer` 1.1.1.
- Primary board support library: official `M5Unified` 0.2.17.
- BLE transport: framework-native Bluedroid BLE HID device APIs; no NimBLE dependency in v1.
- Language: C++17-compatible Arduino C++.
- Target only Cardputer Adv; no legacy M5StickC build.
- No `BOARD_HAS_PSRAM` flag and no PSRAM allocation paths.
- Serial monitor speed: 115200.
- Flash layout: one 3 MB application partition, no OTA, remaining flash assigned to LittleFS after NVS and PHY partitions.
- Wi-Fi is optional at runtime. BLE keyboard functionality must work with no Wi-Fi credentials and no microSD card.

## 5. Architecture

The firmware has five boundaries:

1. **Platform and drivers** initialize M5Cardputer hardware and expose focused wrappers.
2. **Core** owns boot, the event loop, app registration, foreground-app lifecycle, navigation, and input routing.
3. **Services** own long-lived state such as BLE, Wi-Fi, persisted settings, power, storage, and diagnostics.
4. **UI** owns Graphite Mint colors, reusable widgets, status bar, dialogs, and rendering primitives.
5. **Apps** render screens and consume logical input actions through narrow interfaces.

Services do not belong to apps. Opening or closing Keyboard never initializes or destroys the BLE stack. Opening Settings never owns the Wi-Fi driver. Apps request operations from services and render service snapshots.

### 5.1 Proposed source tree

```text
pocket-deck/
├── platformio.ini
├── partitions_8mb.csv
├── README.md
├── docs/
│   └── superpowers/
│       ├── specs/
│       └── plans/
├── include/
│   └── pocket_deck_config.h
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── app.h
│   │   ├── app_registry.h/.cpp
│   │   ├── input.h
│   │   ├── input_router.h/.cpp
│   │   ├── system.h/.cpp
│   │   └── system_context.h
│   ├── drivers/
│   │   ├── board.h/.cpp
│   │   ├── display.h/.cpp
│   │   └── keyboard_matrix.h/.cpp
│   ├── services/
│   │   ├── ble_keyboard_service.h/.cpp
│   │   ├── diagnostics_service.h/.cpp
│   │   ├── power_service.h/.cpp
│   │   ├── settings_store.h/.cpp
│   │   ├── storage_service.h/.cpp
│   │   └── wifi_service.h/.cpp
│   ├── ui/
│   │   ├── theme.h
│   │   ├── status_bar.h/.cpp
│   │   ├── widgets.h/.cpp
│   │   └── quick_settings.h/.cpp
│   └── apps/
│       ├── launcher/launcher_app.h/.cpp
│       ├── keyboard/keyboard_app.h/.cpp
│       └── settings/settings_app.h/.cpp
└── test/
    ├── test_input_router/
    ├── test_keymap/
    └── test_settings_validation/
```

Each `.cpp` has one responsibility. `main.cpp` only creates the system object and delegates `setup()` and `loop()`.

## 6. Core interfaces

### 6.1 App lifecycle

Each app implements:

```cpp
class App {
public:
    virtual ~App() = default;
    virtual const char* id() const = 0;
    virtual const char* title() const = 0;
    virtual void onEnter(SystemContext& context) = 0;
    virtual void onExit(SystemContext& context) = 0;
    virtual void onInput(const InputEvent& event, SystemContext& context) = 0;
    virtual void update(uint32_t nowMs, SystemContext& context) = 0;
    virtual void render(Display& display, const SystemContext& context) = 0;
};
```

The registry holds static app instances and metadata. There is no runtime plugin loading and no heap allocation during app switching.

### 6.2 Input model

Hardware key transitions become `RawKeyState`. `KeyboardMatrix` converts matrix coordinates into physical keys. `InputRouter` then emits one of two outputs:

- Logical `InputEvent` actions for Home, Settings, dialogs, and future apps.
- A complete 8-byte BLE boot-keyboard report while Keyboard is foreground.

G0 is read separately and never appears in a HID report.

### 6.3 Service access

`SystemContext` contains references to services and read-only snapshots used by apps. Apps do not call M5Cardputer, Preferences, Wi-Fi, or BLE APIs directly.

## 7. Navigation and UI

### 7.1 Visual language

- Product name: **Pocket Deck**.
- UI language: English.
- Default theme: **Graphite Mint**.
- Base: dark graphite; primary accent: mint; secondary accent: muted violet; warning: amber; error: coral.
- Typography: compact built-in Latin fonts; no filesystem font is required for v1.
- Screen target: exactly 240×135 landscape.

### 7.2 Home launcher

Home uses a horizontal card carousel. The center card shows the selected app's icon, title, and one status line. Neighboring cards peek in from the sides. Left/right changes selection and Enter opens the selected app. Keyboard is selected by default after every boot.

The status bar shows the current screen title plus Wi-Fi, BLE, and battery states. Time may appear only after network time is available; lack of time must not leave misleading values on screen.

### 7.3 Global controls

- G0 short press: return directly to Home from any app or modal.
- G0 long press: open Quick Settings over the current screen.
- Gesture rule: a long press fires once after 600 ms and suppresses the short action; a short press fires on release before 600 ms. One physical press can never trigger both actions.
- Quick Settings v1 controls: brightness, volume, Wi-Fi enable, BLE keyboard enable, and battery/status summary.
- Backspace in system apps: return one level or cancel a dialog.
- Enter in system apps: open or confirm.
- Arrow actions in system apps come from the Fn layer defined below.

G0 held during power-on remains the hardware download-mode gesture; the runtime mapping does not attempt to override boot ROM behavior.

### 7.4 Boot behavior

Boot never auto-enters Keyboard. After hardware and service initialization, Pocket Deck always displays Home with the Keyboard card selected. BLE may reconnect in the background. The user must press Enter to foreground Keyboard before ordinary matrix keys can reach the Mac.

## 8. Keyboard app and macOS keymap

Keyboard displays connection state, paired host label, encryption state, battery status, and active modifiers. It never displays, logs, persists, or transmits typed text anywhere except as HID reports to the paired host.

### 8.1 Modifier mapping

| Physical key | macOS HID meaning |
|---|---|
| Ctrl | Left Control |
| Shift | Left Shift |
| Opt | Left Option / Left Alt |
| Alt | Left Command / Left GUI |
| Fn | Local layer selector; never sent |

### 8.2 Fn layer

| Combination | HID key |
|---|---|
| Fn + ` | Escape |
| Fn + 1 through 0 | F1 through F10 |
| Fn + - | F11 |
| Fn + = | F12 |
| Fn + Backspace | Forward Delete |
| Fn + Tab | Caps Lock |
| Fn + ; | Up Arrow |
| Fn + , | Left Arrow |
| Fn + . | Down Arrow |
| Fn + / | Right Arrow |

Normal punctuation remains available without Fn. The implementation uses raw physical-key data and its own lookup table because the official library does not map `Opt` into the HID modifier mask and does not translate an Fn layer.

The HID service sends reports on key-state transitions and leaves keys logically held until release. It does not send an immediate `releaseAll()` after every press. A report supports the standard six non-modifier boot-keyboard usages plus the modifier byte.

## 9. BLE keyboard service

### 9.1 Identity and host policy

- One stable BLE identity and one HID GATT profile for the entire firmware lifetime.
- Advertised/device name: `Pocket Deck` by default; user rename changes the name but not the base MAC.
- Exactly one approved host in v1.
- The service starts once during boot and remains initialized while enabled.
- BLE does not reliably expose a human-readable remote computer name to this peripheral. The UI therefore stores a local, user-editable host label with the bond, defaulting to `Mac`, rather than claiming that a discovered peer name is authoritative.

### 9.2 Security and pairing

- LE Secure Connections, encrypted link, bonding, and passkey-based pairing.
- On first pairing, Pocket Deck displays a random six-digit passkey and the user enters it in the macOS pairing prompt. This uses the installed Arduino-ESP32 wrapper's safe DisplayOnly path instead of blocking its synchronous passkey-input callback while waiting for Cardputer keystrokes.
- A successful bond is persisted by the BLE stack in NVS.
- While a host bond exists, Pocket Deck does not silently accept a different host.
- `Forget host & pair new` requires confirmation, disconnects, deletes the old bond, and deliberately re-enters pairing mode.

### 9.3 Connection behavior

- `Disconnect` drops the active link but preserves the bond.
- Disabling Bluetooth stops advertising/connections but preserves the bond.
- Re-enabling Bluetooth resumes advertising with the same identity.
- No key event is buffered while disconnected.
- Immediately after connection and before accepting typing, the service sends an all-keys-up report to prevent stuck modifiers.
- Pairing timeout, authentication failure, and disconnect are visible in the Keyboard card and Bluetooth settings instead of causing a reboot.

## 10. Wi-Fi service

- Wi-Fi configuration is optional and is never part of the mandatory boot flow.
- Settings can enable/disable Wi-Fi, scan nearby networks, show signal/security, connect, forget, and retry.
- Password entry is local to Settings and is never routed to BLE HID.
- Credentials are stored through ESP32 Preferences/NVS and are never written to serial or diagnostics logs.
- Saved Wi-Fi reconnects in the background on later boots.
- A bad password or connection timeout returns a visible error and leaves Settings usable.
- Active scans are user-triggered and rate-limited to avoid unnecessary BLE latency and power drain.

## 11. Settings and persistence

Settings Home is a two-column screen: categories on the left and a status/action summary on the right. Full-screen task pages are used for Wi-Fi scanning, password input, confirmation dialogs, and diagnostics.

Categories:

- Network: enabled state, SSID, signal, IP, scan, connect, forget.
- Bluetooth: enabled state, device name, host, status, disconnect, forget/re-pair.
- Display & Sound: brightness, inactivity timeout, volume, key click.
- Power: battery percentage, sleep policy, low-battery status.
- Storage: internal flash and microSD presence/capacity.
- System: version, language, restart, factory reset, diagnostics.

`SettingsStore` validates a versioned settings record. Missing, older, or invalid records fall back to defaults without reboot loops. BLE bonds remain owned by the BLE stack; application settings do not serialize keys or duplicate bond data.

## 12. Diagnostics and failure behavior

Serial output remains available at 115200, but native USB may miss early logs after reset. Therefore System > Diagnostics also exposes:

- reset reason;
- firmware/build version;
- uptime;
- free heap and minimum free heap;
- BLE state and last BLE error code;
- Wi-Fi state, RSSI, and last Wi-Fi error code without credentials;
- microSD mount state;
- most recent bounded in-memory diagnostic messages.

Failure rules:

- A service failure disables only that service and leaves Home/Settings usable.
- A missing microSD card is a normal state, not an error modal.
- Invalid settings restore defaults and record a diagnostic event.
- Render/update loops avoid unbounded waits and do not allocate every frame.
- Keys pressed while BLE is unavailable are discarded.
- Factory reset clears application settings, Wi-Fi credentials, and the single BLE bond only after explicit confirmation.

## 13. Data flow

```text
TCA8418 keyboard ─→ KeyboardMatrix ─→ InputRouter
                                      ├─→ logical InputEvent ─→ foreground system app
                                      └─→ HID report ─→ BleKeyboardService ─→ paired Mac

Settings UI ─→ service commands ─→ WiFi / BLE / Power / Storage
services ─→ immutable snapshots ─→ status bar, launcher cards, Settings

G0 ─→ System ─→ Home or Quick Settings
```

The foreground app selection controls only routing and rendering. It never changes BLE identity or reconstructs the Bluetooth stack.

## 14. Verification strategy

### 14.1 Host-side tests

Pure C++ tests cover:

- physical key plus modifier/Fn combinations to expected HID reports;
- G0 exclusion from HID;
- local-vs-HID routing based on foreground app;
- disconnected input discard;
- settings defaulting and validation;
- app registry ordering and default selection.

Tests must not include Arduino or M5 headers. Hardware adapters remain thin and receive on-device smoke coverage.

### 14.2 Firmware build checks

- `pio run -e cardputer-adv`
- inspect RAM/flash use and reject accidental PSRAM dependencies;
- `pio run -e cardputer-adv -t upload` on the device;
- serial monitor at 115200 for runtime logs.

### 14.3 On-device acceptance

1. Boot reaches Home with Keyboard selected.
2. G0 short/long actions work from Home, Keyboard, Settings, and dialogs.
3. Wi-Fi scan, wrong-password error, successful connect, reboot reconnect, and forget all work.
4. Initial Mac pairing succeeds by entering Pocket Deck's displayed passkey on the Mac and survives Pocket Deck restart.
5. Ordinary keys, Control/Option/Command/Shift, Fn arrows, F1–F12, Escape, Caps Lock, and Forward Delete match the specification.
6. Holding and releasing key combinations produces correct transitions without stuck modifiers.
7. Disconnect drops input; reconnect begins with all keys released.
8. Forget/re-pair works without erasing flash or using macOS to forget unrelated identities.
9. Removing microSD and disabling Wi-Fi do not affect BLE keyboard operation.
10. The old Claude Buddy repository has no modified files attributable to Pocket Deck work.

## 15. Definition of done

Pocket Deck v1 is done when the source tree and tests match this design, a clean `cardputer-adv` build succeeds, the firmware is flashable, and the on-device acceptance list is complete. Features listed as non-goals remain absent rather than appearing as nonfunctional menu items.
