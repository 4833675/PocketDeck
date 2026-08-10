# Sony IR Remote Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bilingual send-only Sony KD-65X9100H remote using the Cardputer Adv GPIO44 IR emitter and the previously proven SIRC map.

**Architecture:** Keep semantic command mapping portable; let `IrService` own Arduino-IRremote and GPIO44; let `RemoteApp` translate Pocket Deck input into semantic commands and show local transmission status.

**Tech Stack:** C++17 portable tests, Arduino-IRremote 4.7.1, existing input router/localized indexed UI.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck`; archived buddy files are read-only evidence.
- Ship English and Simplified Chinese together and label the TV KD-65X9100H.
- Send one initial Sony frame plus two repeats per key press; do not implement hold-repeat.
- Never claim the TV received a command; this hardware is transmit-only.
- G0 remains the unconditional Pocket Deck Home route while Backspace belongs to TV Back.
- Every code task follows RED -> minimal GREEN -> refactor, then native tests and whitespace checks.

---

## Task 1: Portable Sony command table and app model

**Files:**

- Create `src/core/ir_data.h`.
- Create `src/core/ir_data.cpp`.
- Create `src/apps/remote/remote_app_model.h`.
- Create `src/apps/remote/remote_app_model.cpp`.
- Modify `test/native/test_main.cpp`.
- Modify `scripts/test-native.sh`.

- [ ] Add failing tests for all 13 semantic commands, device 1/151, command hex, 12/15-bit length, and repeat count 2.
- [ ] Add failing model tests for Fn D-pad actions, Enter, Backspace-as-TV-Back, backtick Return, `p/h/i/m/-/=` characters, and ignored input.
- [ ] Implement fixed `SonyIrCode` lookup and `RemoteAppEffect`; do not expose raw keys to `IrService`.
- [ ] Run native tests to GREEN and commit the portable mapping.

## Task 2: Arduino-IRremote service

**Files:**

- Modify `platformio.ini` to add `Arduino-IRremote/IRremote @ ^4.7.1`.
- Create `src/services/ir_service.h`.
- Create `src/services/ir_service.cpp`.
- Modify `src/core/system_context.h`.

- [ ] Define `DISABLE_CODE_FOR_RECEIVER` and include `IRremote.hpp` only in `ir_service.cpp`.
- [ ] Implement idempotent `setActive(bool)` using GPIO44 and `IrSender.begin`/shutdown behavior.
- [ ] Implement `send(SonyIrCommand)` as `IrSender.sendSony(device, command, 2, bits)` and return only local success/failure.
- [ ] Verify the exact library API with a complete firmware build and commit the service.

## Task 3: REMOTE UI and runtime integration

**Files:**

- Create `src/apps/remote/remote_app.h`.
- Create `src/apps/remote/remote_app.cpp`.
- Modify `src/core/app_id.h`.
- Modify `src/core/resource_policy.h`.
- Modify `src/core/system.h`.
- Modify `src/core/system.cpp`.
- Modify `src/apps/launcher/launcher_model.h`.
- Modify `src/apps/launcher/launcher_app.cpp`.
- Modify `test/native/test_main.cpp`.

- [ ] Add failing tests for `AppId::Remote`, `RuntimeResource::Ir`, and launcher reachability/wrap.
- [ ] Register one static `IrService` and `RemoteApp`; activate IR only for REMOTE and shut it down on exit.
- [ ] Use `InputMode::Text`; render bilingual key map, `Sony KD-65X9100H`, last semantic action, and `SENT / 已发送` or local error.
- [ ] Add launcher glyph `IR`; keep G0 global Home behavior unchanged.
- [ ] Run native tests/full build and commit integration.

## Task 4: Documentation and validation

**Files:**

- Create `docs/validation/sony-ir-remote-smoke-test.md`.
- Modify `README.md`.
- Modify `MEMORY.md` without exceeding 200 lines.

- [ ] Document exact controls and the distinction between local `SENT` and TV acknowledgement.
- [ ] Run `scripts/test-native.sh`, `git diff --check`, and `pio run -e cardputer-adv`.
- [ ] If hardware is visible, upload without erase; leave each TV command and cross-app regression pending until observed.

