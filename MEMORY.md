# Pocket Deck project memory

Last updated: 2026-08-10

This is the current handoff for the Pocket Deck repository. Read it with
`AGENTS.md`; use `README.md` for public usage instructions.

## Current state

- Repository: `https://github.com/4833675/PocketDeck`; branch `feature/motion-recorder-ir`, public firmware `0.9.6`.
- The branch adds MOTION / 运动: built-in BMI270 acceleration and gyroscope with LIVE, session-zero LEVEL, and session-peak ACTIVITY; no compass, heading, or raw motion log.
- It also adds REMOTE / 遥控器 for Sony KD-65X9100H only: every mapped press requests one initial Sony frame plus two repeats; `SENT / 已发送` proves only the local call, never TV receipt, and there is no receive, learning, other-brand, or hold-repeat support.
- Motion is foreground-only at 50 Hz / up to 30 Hz render. STILL is `<0.08 g` and `<10 deg/s`; SHAKE is `>=0.45 g` or `>=180 deg/s`, immediate, 500 ms latched; stale is over 500 ms. Physical motion rows pending.
- Native/target evidence: 1,340 checks, 91,096-byte RAM, 2,115,145-byte flash; not proof of physical IR, TV receipt, IMU availability, orientation, rate, zero, shake, or isolation.
- Existing hardware: v0.9.6 SSH over direct Wi-Fi, BLE reconnect, Wi-Fi scan, TF logging, and MEDIA playback; LoRa/GPS/SSH/multi-Wi-Fi pending.

## Product identity

Pocket Deck is a from-scratch, keyboard-first system shell for the **M5Stack
Cardputer Adv**. It is not a Claude Desktop Buddy fork. The archived buddy
firmware may remain in a sibling `../claude-desktop-buddy-cardputer` repository
and must remain untouched unless explicitly selected.

Current apps and services:

- Runtime English / Chinese across every app, Settings, and Quick Settings.
- MOTION / 运动: foreground-only BMI270 accelerometer + gyroscope dashboard;
  no magnetometer/compass, persistent calibration, or raw sample logging.
- REMOTE / 遥控器: Sony KD-65X9100H IR controls only; no receiver, learner,
  other-brand support, hold-repeat, or TV acknowledgement.
- Secure single-host BLE keyboard for macOS, implemented with NimBLE-Arduino
  1.4.3 and a six-digit display-only pairing flow.
- Runtime English / Simplified Chinese four-page GPS dashboard for the optional
  Cap LoRa-1262, including motion validity and stale-data handling.
- Raw LoRa P2P text terminal for the Cap SX1262 and a matching RadioLib peer.
- Foreground MEDIA player with four folder levels, 64 entries per current
  folder, Chinese filenames, ESP8266Audio 2.2.0, and speaker/AUX output.
- Wi-Fi manager with eight profiles, NTP clock, and bilingual GPS-local Open-Meteo weather.
- Pocket SSH Terminal with six NVS host entries, one PTY shell, public-key auth,
  40×13 ANSI display, 64-line scrollback, reconnect, and quick commands.
- Quick Settings, Preferences/NVS settings, on-device diagnostics, and unified
  privacy-safe TF event logs with USB serial dump commands.
- No microphone/dictation, Claude integration, LoRaWAN, encryption/ACK/retry,
  generic IR, SFTP, SSH tunnels, MQTT, or Home Assistant feature yet.

## Hardware facts

- ESP32-S3FN8 / Stamp-S3A, 8 MB flash, no PSRAM, 240×135 display.
- Partition layout: NVS + one 3 MB app + LittleFS; no OTA slot.
- Keyboard: TCA8418 matrix through M5Cardputer 1.1.1.
- GPS Cap: NMEA UART at 115200, Cardputer RX GPIO15 / TX GPIO13.
- Built-in IMU: BMI270 accelerometer + gyroscope; active only for MOTION.
- BLE/Wi-Fi share 2.4 GHz airtime; ESP32-S3 cannot see 5 GHz-only SSIDs.
- SX1262 uses NSS G5, DIO1 G4, reset G3, busy G6 and shared SPI G40/G14/G39.
  TF CS is G12; inactive chip selects stay high and all radio calls run from the
  main system task. The DIO1 callback only sets a volatile flag.
- The Cap antenna switch is PI4IOE5V6408 at I2C `0x43`, P0 high before radio
  initialization. An antenna must be attached before LoRa initialization or TX.
- Native USB Serial/JTAG may disappear briefly on reset and miss early app logs.

## Architecture map

- Entry and lifecycle: `src/main.cpp`, `src/core/system.*`.
- Input/privacy routing: `src/core/input_router.*`, `mac_keymap.*`,
  `text_keymap.*`.
- BLE HID: `src/services/ble_keyboard_service.*`.
- Wi-Fi state machine: `src/services/wifi_service.*`.
- Wi-Fi profile model/store: `src/core/wifi_profiles.*`,
  `src/services/wifi_profile_store.*`.
- GPS/weather: `src/services/gps_service.*`, `weather_service.*`.
- IMU/MOTION: `src/services/imu_service.*`, `src/core/motion_data.*`, and
  `src/apps/motion/`.
- LoRa: `src/core/lora_data.*`, `src/services/lora_service.*`, and
  `src/apps/lora/`; `System` owns the service and calls its update loop.
- MEDIA: `src/core/media_data.*`, `src/services/media_service.*`, and
  `src/apps/media/`; playback objects exist only while a track is loaded.
- SSH: `src/core/ssh_*`, `terminal_*`, `src/services/ssh_*`, and
  `src/apps/ssh/`. Build-time key generation is `scripts/embed_ssh_key.py`.
- UI/apps: `src/apps/`, `src/ui/`, `src/drivers/display.*`.
- Persistent diagnostics: `src/services/diagnostics_service.*`,
  `sd_log_service.*`.
- Durable decisions: `docs/architecture/`.
- Hardware checks: `docs/validation/`.

## Important invariants and lessons

- Keyboard input is forwarded only while Keyboard is foreground and BLE is
  encrypted. Local screens must consume their own keys.
- A reconnect starts with an all-keys-up gate; never replay keys held while the
  Mac was disconnected.
- The BLE identity is stable and only one Mac bond is accepted. The NimBLE
  migration fixed bond loss after leaving radio range; do not casually return
  this service to Bluedroid.
- Wi-Fi credentials are never logged or included in UI snapshots. New profiles
  are persisted only after connection succeeds.
- Before a disconnected scan, cancel a pending association and retry scan start
  while the ESP-IDF driver settles. The previous one-profile implementation
  repeatedly returned `WIFI_SCAN_FAILED` in this state.
- Automatic Wi-Fi candidates must be built from all raw scan records, not only
  the strongest eight shown on screen.
- Weather keeps the last successful response in RAM when GPS or Wi-Fi vanishes;
  only refreshing requires fresh inputs.
- GPS `RX CHARS` alone does not prove valid NMEA. Use checksum OK/ERR and UTC to
  distinguish signal acquisition from serial corruption.
- MOTION reads accel+gyro only in its foreground profile, at a 20 ms minimum
  interval; normal render is 33 ms. Never add compass claims or raw diagnostics.
- LEVEL zero and ACTIVITY peak reset are RAM-only. STILL is strict below both
  0.08 g/10 deg/s, SHAKE inclusive at either 0.45 g/180 deg/s and latches 500 ms;
  non-shake changes require five candidate samples.
- LoRa uses RadioLib 7.7.1 raw P2P only: 868.0 MHz, 125.0 kHz, SF12, 4/5,
  `0x34`, +22 dBm, 20-symbol preamble, 3.0 V TCXO, 140 mA. No custom header,
  encryption, addressing, ACK, retry, persistence, notification, or hidden TX
  queue exists. Do not log drafts, payloads, or message history.
- LoRa initializes lazily on first app entry, listens only while its app is
  foreground, and enters warm sleep with its DIO1 IRQ detached on exit.
  Missing Cap/error must not block GPS, TF, BLE, Wi-Fi, weather, or SSH. RX/TX
  payloads are bounded to 120 printable-ASCII bytes; RX is sanitized and
  oversize input drops. Keep six in-RAM records only.
- MEDIA starts at `/Music`, enters folders one level at a time to depth four,
  keeps 64 fixed-size entries from only the current folder, never remounts TF,
  and stops/releases playback before folder changes or app exit. Chinese names
  use built-in `efontCN_14`; never log filenames or audio content.
- MEDIA uses M5's 1,536-sample speaker buffers and renders at 10 Hz to reduce
  underruns. Recommend 128 kbps / 44.1 kHz; hardware confirmation is pending.
- Runtime language uses one settings flag and compile-time literals. English
  uses `Font0`; Chinese reuses the already-linked `efontCN_14`. Do not add a new
  CJK font size or permanent language branch without a separate budget decision.
- Every new product UI ships English and Chinese together unless technically impossible; document exceptions.
- Preserve commands, protocols, user data, RF/GPS metrics, and raw logs when translation risks changing meaning.
- TF logging normally closes each event so unexpected power loss does not strand
  a long buffered session. MEDIA uses a 12-event categorical deferred queue and
  flushes after its MP3 file closes. `system.log` rotates at 4 MB through three archives; legacy
  `ble*.log` remains dump/clear compatible. Log state changes, never hot loops,
  key/terminal text, passwords, exact coordinates, filenames, or radio payloads.
- The repository never contains the SSH private key. PlatformIO reads an
  external text key and generates its array only under ignored `.pio/`; the
  resulting firmware binary does contain the key and must not be shared.
- SSH host-key verification is deliberately OFF in this prototype. Do not call
  it secure on untrusted networks; TOFU fingerprint storage is the next security
  step. Factory reset clears host records but cannot remove a compiled key.
- SSH uses `aes128-ctr` and 1,024-byte libssh reads; handshake milestones stay in
  RAM until failure. Only pending Wi-Fi auto-retries; other failures wait for Enter.
- libssh runs in one lazy FreeRTOS worker with a 20,480-byte stack, 512-byte TX
  stream, and 1,024-byte RX stream. The generated private key is parsed once at
  task startup and cached for reconnects; parsing it after key exchange failed at
  the heap low-water point. Never call libssh directly from an app or log
  terminal input/output.
- v0.9.2/v0.9.5 logs proved RSA auth was nondeterministic at 13.5 KB free heap.
  v0.9.6's exact-color 8-bit indexed buffer recovers 31,632 runtime bytes without
  shrinking the 20,480-byte SSH stack. Real hardware reached `CONNECTED` and
  opened a shell with 41,240 bytes free and a 23,540-byte largest block.

## Standard workflow

```bash
scripts/test-native.sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

`scripts/embed_ssh_key.py` reads `POCKETDECK_SSH_KEY`, falling back to
`~/.ssh/id_rsa`. A missing key still builds but disables SSH connections.

Serial log commands: `HELP`, `LOG STATUS`, `LOG DUMP`, `LOG DUMP ALL`, and
`LOG CLEAR YES`. Do not clear logs while diagnosing an unresolved issue.

## Near-term validation

1. Run `localization-smoke-test.md` on v0.9.6 across every app and Quick Settings.
2. Run `docs/validation/motion-smoke-test.md`; all axis, zero, stationary,
   shake/latch, exit/isolation, unavailable, and regression rows are pending.
3. Run `resource-isolation-smoke-test.md`; prioritize MEDIA
   continuity, no Wi-Fi/BT work during playback, deferred-log flush, and bond/profile survival.
4. Stay inside SSH or Weather for several minutes and validate direct reconnect,
   scan backoff, and the new disconnect/LOST_IP reason logs.
5. Run `docs/validation/lora-text-terminal-smoke-test.md` with antenna attached
   and a matching second SX1262/RadioLib endpoint; all RF, shared-SPI, and
   regression rows remain pending until observed.
6. Run the new GPS 4/4 moving, stationary, invalid, and stale-motion rows
   outdoors without sharing precise coordinates.
7. Continue the SSH checklist with two more cold boots, then controls, scrollback, and reconnect.
8. Recheck BLE, Wi-Fi, weather, GPS, heap, and UI responsiveness after the SSH
   worker has been created.
9. Connect a second 2.4 GHz network and confirm both entries appear under Saved
   networks and saved scan rows show `S`.
10. Restart near each network separately and confirm automatic connection.
11. Make both visible and confirm strongest-known selection; then make the first
   candidate fail and confirm fallback.
12. Delete one profile and verify the other survives restart.
13. Recheck BLE typing/range-loss reconnect and confirm Wi-Fi scans occur only
   inside SSH, Weather, or Settings.

Use the matching files in `docs/validation/` to record results. After the owner
confirms behavior, update this section, commit locally, and push only if requested.
