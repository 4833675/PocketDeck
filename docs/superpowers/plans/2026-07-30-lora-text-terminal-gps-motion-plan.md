# Pocket Deck LoRa Text Terminal and GPS Motion Implementation Plan

Status: Approved for implementation
Date: 2026-07-30
Design: `docs/superpowers/specs/2026-07-30-lora-text-terminal-gps-motion-design.md`

## Goal

Add a raw RadioLib-compatible SX1262 text terminal for the M5Stack Cap
LoRa-1262 and a dedicated fourth GPS motion page, while preserving all current
Pocket Deck behavior and the user's uncommitted multi-Wi-Fi/SSH work.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck`; never edit either sibling Buddy
  repository.
- Preserve all pre-existing dirty worktree changes. Stage only task-specific
  files or hunks, make local commits only, and never push.
- Target only PlatformIO environment `cardputer-adv` using `~/.platformio`.
- Use RadioLib 7.7.1 and raw LoRa P2P, not LoRaWAN and not a Pocket Deck packet
  wrapper.
- The fixed radio profile is 868.0 MHz, 125.0 kHz bandwidth, SF12, coding rate
  4/5, sync word `0x34`, +22 dBm, 20-symbol preamble, 3.0 V TCXO, and 140 mA
  current limit.
- Cap pins are NSS G5, DIO1 G4, reset G3, busy G6, SCK G40, MOSI G14, MISO G39;
  antenna switch I2C address `0x43` P0 must be driven high before radio init.
- TF card CS G12 and LoRa NSS G5 share SPI. Keep inactive chip selects high;
  all RadioLib calls run from the main system task. The DIO1 callback may only
  set a volatile flag.
- Initialize LoRa lazily on first app entry; afterward keep background receive
  active. A missing Cap or radio error must not block any other app/service.
- Payload limit is exactly 120 bytes of printable ASCII. Transmit exact draft
  bytes with no hidden header. Sanitize non-printable RX bytes to `.` and drop
  oversized RX packets.
- Keep exactly six recent in-RAM records. Do not persist or log drafts, TX/RX
  payloads, keyboard input, credentials, or precise GPS coordinates.
- No encryption, addressing, ACK, retry, delivery guarantee, hidden TX queue,
  NVS persistence, or notifications in this version.
- GPS navigation must wrap across four pages. Page 4 shows only speed in km/h
  and course in degrees plus compass point; invalid or stale values display
  `--`.
- Follow TDD for portable behavior. Every task must leave focused tests green;
  final validation requires `scripts/test-native.sh`, `git diff --check`, and
  `pio run -e cardputer-adv` with pristine test output.
- Do not erase flash, clear NVS/BLE bonds, format the TF card, or alter the
  embedded SSH-key workflow.

## Task 1: Build the portable LoRa text model

### Files

- Create `src/core/lora_data.h`.
- Create `src/core/lora_data.cpp`.
- Modify `scripts/test-native.sh` only as needed to compile the new source.
- Modify `test/native/test_main.cpp` with focused model tests.

### TDD sequence

1. Add tests first and run `scripts/test-native.sh`; record the expected RED
   failure caused by the missing model/API.
2. Implement the smallest portable model that satisfies the tests.
3. Run `scripts/test-native.sh` once for GREEN and `git diff --check`.

### Required behavior

- Define radio states `Unavailable`, `Initializing`, `Listening`,
  `Transmitting`, and `Error`, plus RX/TX direction labels.
- Own a 121-byte null-terminated draft with append, erase, clear, length,
  full/empty, send eligibility, and exact raw-byte copy operations.
- Accept only printable ASCII (`0x20` through `0x7e`) into the local draft.
- Store six bounded message records in chronological order and evict the oldest
  when a seventh arrives.
- Preserve exact printable TX text; replace non-printable RX bytes with `.`.
- Reject an RX packet over 120 bytes, increment dropped count, and add no
  partial history record.
- Track sent, received, CRC-failure, dropped-packet counters, latest valid RSSI
  and SNR, and last RadioLib status code.
- Provide explicit transitions/helpers for initialization, listen, transmit,
  successful TX/RX, CRC failure, recoverable restart, and persistent error so
  these policies remain natively testable.
- Do not use Arduino types, `String`, dynamic containers, or heap allocation.

### Acceptance

- Tests cover draft editing and limit, empty-send rejection, exact payload,
  six-record eviction, direction labels, sanitization, oversized RX rejection,
  counters/quality, and state transitions/recovery.
- Existing native tests remain green and output contains no compiler warnings.
- Commit only Task 1 changes.

## Task 2: Add GPS page 4/4 for speed and course

### Files

- Create `src/apps/gps/gps_app_model.h` and `.cpp` if needed for portable page
  navigation.
- Modify `src/apps/gps/gps_app.h` and `.cpp`.
- Modify `scripts/test-native.sh` and `test/native/test_main.cpp` only for the
  new portable tests.

### TDD sequence

1. Add a native page-navigation test first and capture RED.
2. Implement four-page navigation and rendering.
3. Run focused/full native tests and `git diff --check`.

### Required behavior

- Left wraps `1 -> 4`; Right and Tab wrap `4 -> 1`; Back returns to Launcher.
- Status title is `GPS n/4` on all pages.
- Pages 1-3 preserve their existing content and order.
- Page 4 body contains only a large speed value with `KM/H` and a large course
  value with degree marker and compass point.
- Treat a motion field as unavailable when its validity flag is false or the
  GPS snapshot is stale; render `--`, never a cached misleading value.
- Keep the existing bottom navigation hint compact and unclipped.

### Acceptance

- Native tests prove all four wraparound transitions.
- Firmware-side rendering compiles without changing GPS UART/parser behavior.
- Commit only Task 2 changes.

## Task 3: Integrate the Cap LoRa-1262 service

### Files

- Modify `platformio.ini` to add `jgromes/RadioLib @ 7.7.1`.
- Create `src/services/lora_service.h` and `.cpp`.
- Modify `src/core/system_context.h` only if the service-facing pointer/API is
  needed at this layer.
- Add small portable seams/tests only when needed to validate service policy;
  do not build a fake radio framework.

### Required behavior

- Use `SPI` with the documented shared pins and `SX1262(new Module(5, 4, 3, 6,
  SPI))` or the exact RadioLib 7.7.1 equivalent.
- Before any SPI access, set TF CS G12 and LoRa NSS G5 high, initialize shared
  SPI once using G40/G39/G14, and drive PI4IOE5V6408 P0 high over I2C address
  `0x43`.
- Initialize with the exact fixed profile in Global Constraints, including
  TCXO and current limit. Return a stable unavailable/error model state when
  the Cap cannot be initialized.
- `ensureStarted()` is lazy and idempotent. Once initialized, `update()` keeps
  receive active even if the LoRa app is no longer foreground.
- DIO1 ISR/callback only sets a volatile completion flag. Process packet
  length, RX read, TX completion, errors, state mutation, and all SPI operations
  from `update()`.
- Receive at most 120 bytes. Count CRC mismatch, drop oversize packets, capture
  RSSI/SNR, and re-arm receive immediately.
- Transmit one non-empty listening-state draft with non-blocking
  `startTransmit()`. Ignore/reject concurrent send requests without queueing or
  duplication; record TX only after successful completion.
- After a non-CRC operation error, try one standby/restart-receive recovery and
  expose a persistent error plus raw RadioLib code if recovery fails.
- Emit diagnostics only for state/error/code/length/counters; never payload.

### Validation

- Run `scripts/test-native.sh`, `git diff --check`, and a complete
  `pio run -e cardputer-adv` to prove the exact RadioLib APIs and board build.
- Review static RAM/flash growth; do not claim hardware radio success yet.
- Commit only Task 3 changes.

## Task 4: Add the LoRa app and system wiring

### Files

- Create `src/apps/lora/lora_app.h` and `.cpp`.
- Modify `src/core/app_id.h`.
- Modify `src/apps/launcher/launcher_model.h` and launcher rendering.
- Modify `src/core/system.h`, `src/core/system.cpp`, and
  `src/core/system_context.h` as required.
- Modify native test/build files for launcher and integration-policy coverage.

### TDD sequence

1. Add/adjust launcher tests to expect six apps and wraparound; capture RED.
2. Add app/system wiring and make the tests GREEN.
3. Run the complete native suite, whitespace check, and firmware build.

### Required behavior

- Add `AppId::LoRa` and place a `LORA` card in the six-app launcher without
  disturbing existing app order more than necessary.
- `System` statically owns one `LoRaService` and one `LoRaApp`, exposes the
  service through `SystemContext`, calls service `update()` every loop, and
  routes app entry/exit through the existing lifecycle.
- Entering LoRa calls lazy initialization; leaving clears only the unsent draft
  and retains history/background receive.
- The app uses `InputMode::Text`: printable ASCII appends, Backspace erases,
  Enter sends a non-empty draft only while listening, and Fn+Backtick returns
  to Launcher.
- Render status title `LORA`, profile/state row (`868.0 SF12` plus state), six
  recent `RX>`/`TX>` records with newest visible at bottom, latest RX RSSI/SNR,
  a bounded bottom draft, and `ENTER SEND` hint within 240x135.
- Give visible feedback for missing radio, persistent error, and rejected busy
  send without blocking the rest of the system.
- Never send/log payloads outside the LoRa service/model path.

### Acceptance

- Native launcher tests cover six entries and both-direction wraparound.
- Full native and firmware builds pass; existing Keyboard, SSH, GPS, Weather,
  and Settings remain reachable by inspection/tests.
- Commit only Task 4 changes.

## Task 5: Document and validate the integrated feature

### Files

- Create `docs/validation/lora-text-terminal-smoke-test.md`.
- Update `docs/validation/gps-smoke-test.md` for page 4.
- Update `README.md` and `MEMORY.md` without claiming unperformed hardware
  checks; keep `MEMORY.md` under 200 lines.
- Touch other documentation only where a now-false statement must be corrected.

### Required behavior

- Document the exact profile, antenna-required warning, second compatible
  SX1262/RadioLib endpoint requirement, and no-ACK/no-encryption limitations.
- Hardware checklist covers antenna switch/Cap detection, exact bidirectional
  payloads, 120-byte bound, RSSI/SNR, repeated RX re-arm, duplicate-send guard,
  CRC/error recovery, TF coexistence/restart, and regressions in BLE/Wi-Fi/SSH.
- GPS checklist covers page 4 while moving, stationary, invalid, and stale.
- Update current project state and architecture index without adding session
  history or exposing credentials/coordinates/payloads.

### Final validation

1. Run `scripts/test-native.sh` and record total checks.
2. Run `git diff --check`.
3. Run `pio run -e cardputer-adv` and record RAM, flash, and warnings.
4. If a serial device is present, upload without erase and perform only safe
   smoke checks available with one endpoint. Do not transmit unless an antenna
   is attached; do not pretend two-endpoint interoperability was tested.
5. Inspect `git status` and ensure `connect.d`, prior user changes, secrets, and
   generated files were not accidentally staged.

### Acceptance

- Documentation accurately separates compiled/native-tested behavior from
  hardware-tested behavior.
- All automated checks pass and any untestable hardware rows remain pending.
- Commit only Task 5 documentation/test adjustments. Do not push.
