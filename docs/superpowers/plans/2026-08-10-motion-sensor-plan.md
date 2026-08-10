# MOTION Sensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bilingual, foreground-only BMI270 instrument with live axes, a zeroable level, and stable activity classification.

**Architecture:** Keep calculations and navigation portable in `motion_data` and `motion_app_model`; let `ImuService` be the only M5Unified IMU owner; wire it through the existing app/resource lifecycle.

**Tech Stack:** C++17 portable tests, Arduino/M5Unified, existing indexed-canvas UI and runtime localization.

## Global Constraints

- Work only in `/Users/kx/M5Stack/pocket-deck`.
- Ship English and Simplified Chinese together.
- Never reinitialize shared I2C while switching apps and never sample outside the MOTION profile.
- Use fixed storage only; log lifecycle/errors, never samples or gestures.
- Preserve existing app behavior and the SSH heap budget.
- Every code task follows RED -> minimal GREEN -> refactor, then `scripts/test-native.sh` and `git diff --check`.

---

## Task 1: Portable motion calculations and state machine

**Files:**

- Create `src/core/motion_data.h`.
- Create `src/core/motion_data.cpp`.
- Modify `test/native/test_main.cpp`.
- Modify `scripts/test-native.sh`.

- [ ] Add failing tests for roll/pitch formula, alpha-0.2 filtering, exact still/shake boundaries, five-sample still/moving hysteresis, 500 ms shake latch, and peak reset.
- [ ] Run `scripts/test-native.sh` and confirm RED from missing APIs.
- [ ] Implement `MotionSample`, `MotionActivity`, `MotionClassifier`, vector magnitudes, and level calculation without Arduino types or dynamic allocation.
- [ ] Run the native suite to GREEN and commit the portable layer.

## Task 2: Portable app navigation

**Files:**

- Create `src/apps/motion/motion_app_model.h`.
- Create `src/apps/motion/motion_app_model.cpp`.
- Modify `test/native/test_main.cpp`.
- Modify `scripts/test-native.sh`.

- [ ] Add failing tests proving Tab/Left/Right wrap three pages, Enter zero/reset semantics, and Back requests Home.
- [ ] Implement fixed `MotionPage {Live, Level, Activity}` navigation and effects.
- [ ] Run native tests to GREEN and commit the model.

## Task 3: M5Unified IMU service

**Files:**

- Create `src/services/imu_service.h`.
- Create `src/services/imu_service.cpp`.
- Modify `src/core/system_context.h`.

- [ ] Define a fixed `ImuSnapshot` containing availability, freshness, raw axes, filtered orientation, magnitudes, activity, and peak.
- [ ] Implement idempotent `begin()`, `setActive(bool)`, `update(nowMs)` at 20 ms, `zeroLevel()`, and `resetPeak()` using `M5Cardputer.Imu.getAccel/getGyro` only while active.
- [ ] Detect unavailable IMU without blocking startup or other apps; do not call Wire begin/end.
- [ ] Add only lifecycle/init diagnostics at the system boundary.
- [ ] Build `cardputer-adv` to verify the exact M5Unified API and commit the service.

## Task 4: MOTION UI and system integration

**Files:**

- Create `src/apps/motion/motion_app.h`.
- Create `src/apps/motion/motion_app.cpp`.
- Modify `src/core/app_id.h`.
- Modify `src/core/resource_policy.h`.
- Modify `src/core/system.h`.
- Modify `src/core/system.cpp`.
- Modify `src/apps/launcher/launcher_model.h`.
- Modify `src/apps/launcher/launcher_app.cpp`.
- Modify `test/native/test_main.cpp`.

- [ ] Add failing resource/launcher tests for `AppId::Motion` and `RuntimeResource::Imu`.
- [ ] Register static `ImuService`/`MotionApp`, expose the service in `SystemContext`, call update each loop, and activate it only for MOTION.
- [ ] Render bilingual LIVE, LEVEL, and ACTIVITY pages at normal 30 Hz, including bounded bubble and `IMU UNAVAILABLE / IMU 不可用`.
- [ ] Add launcher glyph `IM`; preserve deterministic carousel wrap.
- [ ] Run native tests and full firmware build, then commit integration.

## Task 5: Documentation and validation

**Files:**

- Create `docs/validation/motion-smoke-test.md`.
- Modify `README.md`.
- Modify `MEMORY.md` by replacing/compressing text so it remains under 200 lines.

- [ ] Document controls, no-compass limitation, thresholds, session-only zero, and pending hardware checks in both UI languages where user-facing.
- [ ] Run `scripts/test-native.sh`, `git diff --check`, and `pio run -e cardputer-adv`.
- [ ] If a device is visible, upload without erase; leave axis/zero/stability/shake/regression rows pending until observed on hardware.

