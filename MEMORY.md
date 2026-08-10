# Pocket Deck project memory

Last updated: 2026-08-10

Read this with `AGENTS.md` before substantial work. `README.md` is the public
guide; `docs/validation/` holds reusable hardware checklists.

## Current state

- Repository: `https://github.com/4833675/PocketDeck`; public version remains
  `0.9.6` until the user explicitly requests a GitHub sync.
- RECORDER runtime integration is commit `d12f8fd`, with review fixes in
  `50aa619` and deferred-log hardening in `7799301`; documentation is in
  `518cb22` and `6485819`.
- Automated final-integrated evidence: `scripts/test-native.sh` passed 1,855
  checks; `git diff --check` passed; `pio run -e cardputer-adv` passed with RAM
  `104824 / 327680` bytes (32.0%) and flash `2133097 / 3145728` bytes (67.8%).
  This is source/build evidence only, not physical hardware proof.
- Physical Recorder checks are all pending. Do not infer microphone capture,
  WAV finalization, speaker/AUX playback, recovery, resource isolation, or
  cross-app behavior from a build.
- Existing hardware history: v0.9.6 SSH over direct Wi-Fi, BLE range-loss
  reconnect, Wi-Fi scan, TF logging, and MEDIA playback have been observed.
  LoRa, GPS, SSH breadth, multi-Wi-Fi, MOTION, REMOTE, and RECORDER checklists
  still contain pending rows until directly observed.

## Product and hardware

- Pocket Deck is from-scratch keyboard-first firmware for **M5Stack Cardputer
  Adv** only, not a Claude Desktop Buddy fork. Preserve sibling archived buddy
  repositories.
- Target: Stamp-S3A / ESP32-S3FN8, 8 MB flash, about 320 KB SRAM, **no PSRAM**,
  240×135 display. Partitioning has one 3 MB app and no OTA slot.
- The exact-color 8-bit display buffer was chosen to leave enough runtime heap
  for RSA-backed SSH. Avoid whole-file buffers, unbounded containers, or extra
  tasks without a new memory budget and hardware proof.
- BLE and Wi-Fi share 2.4 GHz airtime; only 2.4 GHz Wi-Fi is usable.
- Optional Cap LoRa-1262 provides GNSS and SX1262 LoRa. GNSS UART is RX GPIO15 /
  TX GPIO13 at 115200. LoRa and TF share SPI; keep inactive chip-select lines
  high and perform work on the main system task.
- Native USB Serial/JTAG re-enumerates after reset, so persistent TF logs are
  more reliable than a late-attached serial monitor for early boot history.

## Apps and services

- Runtime English/Simplified Chinese covers all product-owned UI. Preserve
  commands, protocol values, filenames, radio payloads, metrics, and raw logs.
  English uses `Font0`; Chinese reuses linked `efontCN_14`. Do not add another
  CJK font or a permanent language branch without a budget decision.
- Keyboard: NimBLE HID, one bonded Mac, authenticated/encrypted foreground-only
  reports, all-keys-up reconnect gate. Do not casually return to Bluedroid.
- Wi-Fi: eight successful profiles, strongest-known candidate selection and
  fallback; credentials persist only after `WL_CONNECTED` and are never logged.
- GPS/Weather: four GPS pages, NTP clock, GPS-local Open-Meteo cache in RAM.
  `RX CHARS` alone is not valid NMEA; use checksum counters and UTC.
- MOTION / 运动: foreground-only BMI270 accel+gyro dashboard, no compass or raw
  logs. Sample no faster than 20 ms; stale after 500 ms. STILL is below both
  `0.08 g` and `10 deg/s`; SHAKE is at least either `0.45 g` or `180 deg/s`,
  immediate and latched 500 ms; other changes need five candidate samples.
- REMOTE / 遥控器: fixed Sony KD-65X9100H IR only. Each press is initial frame
  plus two repeats. `SENT / 已发送` is local-call status, never TV receipt; no
  receiver, learner, other-brand mode, or hold repeat exists.
- LORA: lazy foreground raw P2P only: 868.0 MHz, 125 kHz, SF12, 4/5, `0x34`,
  +22 dBm, 20-symbol preamble, 3.0 V TCXO, 140 mA. No LoRaWAN, encryption, ACK,
  retry, persistence, or hidden TX queue. Attach antenna before initialization.
- MEDIA: `/Music`, four folder levels, up to 64 current-folder entries, Chinese
  names, foreground MP3 only. It owns Speaker while active and releases it
  before folder change or exit. Recommend 128 kbps / 44.1 kHz.
- RECORDER / 录音机: `/Recordings`; RIFF/WAVE PCM mono signed-16 16 kHz,
  32 KB/s (about 115 MB/hour). It displays at most 64 regular `.WAV` files,
  newest filename first, and plays only its own format.
- SSH: six host entries, public-key auth, one 40×13 PTY and 64-line scrollback.
  Host-key verification is deliberately OFF in this prototype; do not describe
  it as secure on an untrusted network. The key is external at build time but
  embedded in firmware; never commit, print, or publish it.

## Architecture and ownership

- `src/core/` is portable models/policies; `src/services/` owns hardware-facing
  state; `src/apps/` owns UI/input; `System` owns lifecycle/resource changes.
  Apps request work through `SystemContext`, not direct radio/storage drivers.
- Fixed arrays and explicit state machines are preferred to heap allocation and
  blocking loops. Native tests should cover new portable policy/model behavior.
- Resource profiles turn on only what a foreground app requires. MEDIA and
  RECORDER realtime modes suspend BLE, Wi-Fi, GPS, and LoRa work and defer TF
  diagnostics; deferred diagnostics flush only after active audio files close.
- MEDIA is one intentional long-lived audio/TF exception: one read-only MP3 may
  stay open only during foreground playback.
- RECORDER is the second intentional long-lived audio/TF exception. It uses the
  already-mounted SD instance only: no remount, format, repair, extra task,
  `String`, vector, or whole-recording allocation.
- RECORDER owns Mic or Speaker exclusively. It has exactly two 1,024-sample
  capture buffers and three fixed playback buffers. Start record stops Speaker;
  stop/error/exit drains safely, ends Mic, finalizes/closes WAV, restores Speaker
  and persisted volume before the next app profile applies.
- Recorder starts with a placeholder 44-byte WAV header, tracks successful PCM
  bytes, and checkpoints header/flush every 64 KiB (about two seconds). Normal
  stop, recoverable error, and app exit finalize synchronously. A sudden power
  loss can lose the final in-flight buffer but should leave the latest checkpoint
  length usable.
- Recorder diagnostics are categorical lifecycle/error events plus aggregate
  byte/duration totals. During realtime audio, only exact whitelisted Recorder
  scan/state/exit fields enter the fixed deferred queue; failed writes retain
  queue order for retry. Never log filenames, directory listings, audio samples,
  waveform/level values, recorded content, typed text, passwords, passkeys,
  precise coordinates, or LoRa payloads.
- Upstream limitations: Arduino `File::flush()` has no result, M5Unified hides
  task-create success, and the conservative Mic wake timeout may drop an
  unobserved completed block rather than risk writing a buffer still owned by Mic.

## Important operational lessons

- Before a disconnected Wi-Fi scan, cancel pending association and retry scan
  start while ESP-IDF settles. Build automatic candidates from all raw results,
  not just the eight visible rows.
- Weather retains the last successful response in RAM when GPS/Wi-Fi vanishes;
  only a refresh needs fresh inputs.
- LoRa initializes on first app entry, listens only foreground, then warm-sleeps
  on exit. Missing Cap/error must not block GPS, TF, BLE, Wi-Fi, Weather, or SSH.
- TF logs are state-change only, rotate at 4 MB through three archives, and
  normally close each event. Do not add per-frame, per-key, audio, terminal,
  filename, or payload logging.
- SSH uses `aes128-ctr`, 1,024-byte reads, a lazy 20,480-byte worker stack,
  512-byte TX and 1,024-byte RX streams. Parsing the compiled key once at task
  startup avoids a post-key-exchange heap low point.
- v0.9.6 hardware reached SSH `CONNECTED` with 41,240 bytes free and a 23,540-
  byte largest block. Treat every firmware-size or memory allocation change as
  potentially relevant to SSH reliability.

## Standard workflow

```bash
scripts/test-native.sh
git diff --check
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

- Use PlatformIO packages under `~/.platformio` and only `cardputer-adv`.
- Ordinary update: power off, connect USB, then upload. Use G0 download mode
  only if the serial port is absent or ordinary upload fails.
- Do not erase flash, clear NVS/BLE bonds, format TF, delete user recordings,
  or push to GitHub without explicit owner authorization.
- `scripts/embed_ssh_key.py` reads `POCKETDECK_SSH_KEY`, otherwise
  `~/.ssh/id_rsa`; missing key builds but disables SSH. Generated key data stays
  under ignored `.pio/` and must never be staged.

## Near-term validation

1. Run `docs/validation/recorder-smoke-test.md`: all physical rows are pending,
   including 30-second/multi-minute capture, final WAV/playback, error recovery,
   exit cleanup, checkpoints, MEDIA/system sounds/logs, LoRa, BLE/Wi-Fi/GPS/IR,
   and SSH public-key authentication after Recorder use.
2. Run the MOTION, Sony REMOTE, LoRa, GPS, MEDIA, localization, resource-
   isolation, and TF-log checklists; retain Pending until actually observed.
3. Validate SSH direct reconnect, Wi-Fi scan backoff and reconnect over several
   minutes, then recheck BLE typing/range-loss reconnect and persisted profiles.
4. After owner confirmation, update only observed checklist rows, commit locally,
   and push only when explicitly requested.
