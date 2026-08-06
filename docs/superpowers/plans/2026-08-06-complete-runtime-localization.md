# Complete Runtime Localization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` or `superpowers:executing-plans` to
> implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Finish runtime English / Simplified Chinese UI for all existing Pocket
Deck screens and make bilingual delivery the default for future features.

**Architecture:** Keep one firmware and one persisted `UiLanguage`. Add pure,
fixed-string text adapters beside apps whose service enums/details need mapping;
renderers consume those adapters and explicitly select UI or technical fonts.

**Tech Stack:** C++17 native tests, Arduino ESP32 / PlatformIO,
M5GFX `Font0` and `efontCN_14`, fixed-size buffers and compile-time literals.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck` and preserve all existing dirty
  changes.
- Do not change input bindings, radio policies, SSH commands, payloads, user
  data, logs, or persistent settings format.
- Keep `MEMORY.md` under 200 lines.
- Do not commit or push; the user controls synchronization.
- Do not erase NVS, BLE bonds, Wi-Fi profiles, or TF contents.

---

### Task 1: Pure app text adapters

**Files:**
- Create: `src/core/ssh_data.h`
- Modify: `src/services/ssh_service.h`
- Create: `src/apps/keyboard/keyboard_app_text.h`
- Create: `src/apps/keyboard/keyboard_app_text.cpp`
- Create: `src/apps/ssh/ssh_app_text.h`
- Create: `src/apps/ssh/ssh_app_text.cpp`
- Create: `src/apps/lora/lora_app_text.h`
- Create: `src/apps/lora/lora_app_text.cpp`
- Create: `src/apps/media/media_app_text.h`
- Create: `src/apps/media/media_app_text.cpp`
- Create: `src/apps/settings/settings_app_text.h`
- Create: `src/apps/settings/settings_app_text.cpp`
- Modify: `scripts/test-native.sh`
- Modify: `test/native/test_main.cpp`

**Interfaces:**

```cpp
const char* localizedKeyboardStateLabel(BleKeyboardState, UiLanguage);
const char* localizedKeyboardErrorLabel(BleKeyboardError, UiLanguage);
const char* localizedSshStateLabel(SshState, UiLanguage);
const char* localizedSshErrorLabel(SshError, UiLanguage);
const char* localizedLoRaStateLabel(LoRaRadioState, UiLanguage);
const char* localizedMediaStateLabel(MediaPlaybackState, UiLanguage);
const char* localizedMediaDetailLabel(const char*, UiLanguage);
const char* localizedResetReasonLabel(const char*, UiLanguage);
const char* localizedStorageErrorLabel(const char*, UiLanguage);
```

- [ ] Move `SshState`, `SshError`, and `SshSnapshot` unchanged into portable
      `core/ssh_data.h`; run the existing native suite before adding new tests.
- [ ] Add headers, native-source entries, and literal assertions for every enum
      branch and known detail string.
- [ ] Run `scripts/test-native.sh` and confirm link failure for the absent
      adapter implementations.
- [ ] Implement only the fixed mappings; preserve unknown detail strings.
- [ ] Run `scripts/test-native.sh` and confirm the complete suite passes.

### Task 2: Keyboard and Quick Settings

**Files:**
- Modify: `src/apps/keyboard/keyboard_app.cpp`
- Modify: `src/ui/quick_settings.h`
- Modify: `src/ui/quick_settings.cpp`
- Modify: `src/core/system.cpp`

- [ ] Derive language from `context.settings` in Keyboard and localize the title,
      states, pairing/connected/advertising details, known errors, and hint.
- [ ] Preserve passkey and modifier key names as technical content.
- [ ] Pass `UiLanguage` to `QuickSettings::render` and localize overlay title,
      battery, brightness, volume, Bluetooth state, and compact hint.
- [ ] Run `scripts/test-native.sh`.

### Task 3: SSH product chrome

**Files:**
- Modify: `src/apps/ssh/ssh_app.cpp`

- [ ] Thread `UiLanguage` through hint, host-list, editor, centered-state, and
      terminal-history overlay helpers.
- [ ] Localize missing-key instructions, empty hosts, CRUD hints, validation,
      delete confirmation, connection states/errors, host-key warning, retry
      countdown, and disconnect actions.
- [ ] Keep terminal bytes, ANSI grid geometry, host values, `NVS!`, and the four
      shell command strings unchanged and rendered with the technical font.
- [ ] Run `scripts/test-native.sh`.

### Task 4: LoRa and MEDIA

**Files:**
- Modify: `src/apps/lora/lora_app.cpp`
- Modify: `src/apps/media/media_app.cpp`

- [ ] Localize LoRa title, radio state, unavailable/busy text, and hint while
      preserving payload rows and RF metrics.
- [ ] Localize MEDIA title, playback/empty/error states, known service details,
      ready/volume status, rescan/navigation/playback hints.
- [ ] Keep names, paths, elapsed time, percentages, folder markers, and progress
      geometry unchanged; select fonts explicitly around each region.
- [ ] Run `scripts/test-native.sh`.

### Task 5: Existing localized screens and shared policy

**Files:**
- Modify: `src/apps/settings/settings_app.cpp`
- Modify: `src/apps/launcher/launcher_app.cpp`
- Modify: `AGENTS.md`

- [ ] Localize Settings reset-reason presentation, TF errors, and the readable
      labels on Wi-Fi/diagnostics technical pages; preserve raw log rows.
- [ ] Reuse GPS and Weather localized adapters in Launcher subtitles.
- [ ] Add the repository rule that every new user-visible feature ships English
      and Simplified Chinese together unless technically impossible.
- [ ] Run `scripts/test-native.sh` and `git diff --check`.

### Task 6: Release documentation and long-term memory

**Files:**
- Modify: `include/pocket_deck_config.h`
- Modify: `README.md`
- Modify: `docs/architecture/0006-runtime-localization.md`
- Modify: `docs/validation/localization-smoke-test.md`
- Modify: `docs/validation/ble-keyboard-smoke-test.md`
- Modify: `docs/validation/ssh-terminal-smoke-test.md`
- Modify: `docs/validation/lora-text-terminal-smoke-test.md`
- Modify: `docs/validation/media-mp3-smoke-test.md`
- Modify: `MEMORY.md`
- Create: `/Users/kx/.codex/memories/extensions/ad_hoc/notes/20260806-101504-pocket-deck-bilingual-default.md`

- [ ] Increment the firmware version and document complete runtime localization.
- [ ] Add bilingual rows for Keyboard, SSH, LoRa, MEDIA, and Quick Settings;
      leave hardware results pending.
- [ ] Update current test/build figures after final verification and keep
      `MEMORY.md` below 200 lines.
- [ ] Write the durable Codex memory note with the same translation boundary and
      exception rule.

### Task 7: Final verification and deployment

**Files:** no new files.

- [ ] Run `scripts/test-native.sh`; require zero failures.
- [ ] Run `git diff --check`; require no output and exit code zero.
- [ ] Run `pio run -e cardputer-adv`; record RAM and Flash usage.
- [ ] Re-read this plan and verify every visible-string category is covered.
- [ ] If `/dev/cu.usbmodem*` exists, run
      `pio run -e cardputer-adv -t upload` without erase or `uploadfs`.
- [ ] Report automated evidence separately from pending 240×135 visual checks.
