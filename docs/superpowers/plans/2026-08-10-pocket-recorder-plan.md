# Pocket Recorder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bilingual streaming WAV recorder/player that uses fixed memory, the existing TF mount, and exclusive foreground audio ownership.

**Architecture:** Keep WAV encoding/validation, filename choice, bounded file list, and UI transitions portable. `RecorderService` owns fixed PCM buffers, File handles, Mic/Speaker transitions, and safe header checkpoints. `RecorderApp` owns interaction and rendering; `RecorderRealtime` isolates competing workloads and defers diagnostics.

**Tech Stack:** C++17 portable tests, Arduino FS/SD, M5Unified Mic/Speaker, existing indexed UI and localization.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck`.
- Ship English and Simplified Chinese together; never translate or log filenames.
- Record RIFF PCM mono signed-16 at 16 kHz under `/Recordings`.
- Use exactly two fixed 1,024-sample capture buffers; no `String`, vector, whole-file allocation, second SD mount, or recorder-created task.
- Recorder foreground disables BLE/Wi-Fi/GPS/LoRa and defers TF diagnostics before files open.
- Do not format, remount, repair, or erase the user's TF card.
- Every code task follows RED -> minimal GREEN -> refactor, then native tests and whitespace checks.

---

## Task 1: Portable WAV and recording filename helpers

**Files:**

- Create `src/core/recorder_data.h`.
- Create `src/core/recorder_data.cpp`.
- Modify `test/native/test_main.cpp`.
- Modify `scripts/test-native.sh`.

- [ ] Add failing tests for the 44-byte RIFF header at zero/checkpoint/final sizes, little-endian fields, PCM mono/16-bit/16 kHz validation, malformed/unsupported rejection, wall-clock filename formatting, and sequential fallback candidates.
- [ ] Implement byte-array header build/parse and fixed-capacity naming helpers without Arduino dependencies.
- [ ] Run native tests to GREEN and commit WAV helpers.

## Task 2: Portable bounded library and recorder app state

**Files:**

- Create `src/apps/recorder/recorder_app_model.h`.
- Create `src/apps/recorder/recorder_app_model.cpp`.
- Extend `src/core/recorder_data.h` and `.cpp` with fixed recording entries.
- Modify `test/native/test_main.cpp`.
- Modify `scripts/test-native.sh`.

- [ ] Add failing tests for `.WAV` filtering, newest-name-first ordering, 64-entry cap, wrapped selection, Record/Files page switching, record/play toggles, delete confirmation, Backspace cancel/Home, and exit cleanup effect.
- [ ] Implement fixed arrays and explicit `RecorderAppEffect` state transitions.
- [ ] Run native tests to GREEN and commit the portable model.

## Task 3: Streaming RecorderService

**Files:**

- Create `src/services/recorder_service.h`.
- Create `src/services/recorder_service.cpp`.
- Modify `src/core/system_context.h`.

- [ ] Define fixed `RecorderSnapshot`, state/error enums, 2x1024 `int16_t` capture buffers, fixed playback chunk, and 64-entry library.
- [ ] Implement `scan(storageMounted)`: use the existing global `SD`, create `/Recordings` only if mounted, list regular `.WAV` files, and cache card free space.
- [ ] Implement collision-free wall-clock/sequential file creation, placeholder header, successful-byte tracking, and 2-second seek/write/flush checkpoints.
- [ ] Implement double-buffer capture by queueing only a free buffer to M5Unified Mic, writing completed buffers on the main task, and safely finalizing on stop/short-write/card-full/exit.
- [ ] Implement audio ownership: stop Speaker -> begin Mic for record; end Mic -> finalize -> begin Speaker -> restore persisted volume; Mic stays off during playback.
- [ ] Implement bounded WAV validation and fixed-chunk 16 kHz playback through M5Unified Speaker; close unsupported/damaged files immediately.
- [ ] Implement safe delete only after explicit model confirmation; diagnostics expose categories and byte/duration totals but never names or content.
- [ ] Compile repeatedly against the exact M5Unified/FS APIs, inspect RAM growth, and commit the service.

## Task 4: RECORDER UI and resource integration

**Files:**

- Create `src/apps/recorder/recorder_app.h`.
- Create `src/apps/recorder/recorder_app.cpp`.
- Create `src/apps/recorder/recorder_app_text.h`.
- Create `src/apps/recorder/recorder_app_text.cpp`.
- Modify `src/core/app_id.h`.
- Modify `src/core/resource_policy.h`.
- Modify `src/core/system.h`.
- Modify `src/core/system.cpp`.
- Modify `src/apps/launcher/launcher_model.h`.
- Modify `src/apps/launcher/launcher_app.cpp`.
- Modify `test/native/test_main.cpp`.

- [ ] Add failing tests for `AppId::Recorder`, `RuntimeResource::RecorderRealtime`, launcher reachability, and isolation from all radios/GPS/LoRa.
- [ ] Register static service/app, expose service/context, update only in the system loop, and treat recorder realtime exactly like media for log deferral.
- [ ] Ensure `onExit` synchronously stops/finalizes audio before `System::openApp` changes the profile.
- [ ] Render bilingual Record and Files pages, red recording state, waveform/level, elapsed/bytes/free space, four-row filename list, unsupported/no-card/storage errors, and separate delete confirmation.
- [ ] Add launcher glyph `RC`; use normal 30 Hz render unless measured hardware evidence requires a slower recorder interval.
- [ ] Run native tests/full build and commit integration.

## Task 5: Documentation and full validation

**Files:**

- Create `docs/validation/recorder-smoke-test.md`.
- Modify `README.md`.
- Modify `AGENTS.md` for the second intentional long-lived audio implementation unit and recorder invariants.
- Modify `MEMORY.md` by replacing/compressing existing text so it remains under 200 lines.

- [ ] Document storage rate, controls, file format, privacy, audio ownership, and unsupported WAV scope.
- [ ] Run `scripts/test-native.sh`, `git diff --check`, and `pio run -e cardputer-adv`; compare static RAM/flash and preserve SSH budget.
- [ ] If the device is visible, upload without erase. Hardware rows for 30-second/multi-minute record, playback/AUX, no-card, app-exit finalization, deletion, power interruption, card-full, MEDIA, sounds, TF logs, LoRa SPI, BLE/Wi-Fi/GPS/IR/SSH remain pending until directly observed.

