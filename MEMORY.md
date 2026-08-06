# Pocket Deck project memory

Last updated: 2026-08-06

This is the current handoff for the Pocket Deck repository. Read it with
`AGENTS.md`; use `README.md` for public usage instructions.

## Current state

- Repository: `https://github.com/4833675/PocketDeck`, branch `main`.
- Current `v0.9.6` localizes all product UI and adds deterministic SSH heap headroom.
- v0.9.6 reached a public-key shell over direct Wi-Fi on real hardware.
- Automated validation passed 1,104 checks; the build used 90,952 bytes RAM
  (27.8%) and 2,099,445 bytes flash (66.7%).
- No LoRa Cap, antenna, RF, or two-endpoint hardware claim has been made.
- A MEDIA `tracks=0` incident was traced from TF evidence to stale v0.6 firmware,
  whose scanner was non-recursive. v0.7.0 and then v0.7.1 were uploaded without
  clearing NVS; USB `HELP` confirmed the new `system.log` command path.
- Real-device logs confirmed BLE reconnect, successful Wi-Fi scans, and a manual
  Tab scan (`raw=29`, strongest 8 displayed). The one migrated saved SSID was
  not visible at the test location (`candidates=0`), which is expected.
- Still awaiting user validation: the LoRa two-endpoint checklist, GPS 4/4
  moving/stationary/invalid/stale checks, remaining Pocket SSH rows, second-Wi-Fi
  save, restart/reconnect, strongest-known selection, fallback, and deletion.
- v0.8.0 was uploaded without erasing NVS; USB `LOG STATUS` reported TF logging
  ready/mounted. MEDIA playback is now smooth and BLE Keyboard works after
  app-scoped suspend/resume, as confirmed on the real device.
- v0.9.1 gives a dropped link a direct reconnect before profile scans, applies
  2/5/15/30-second scan backoff, and logs driver disconnect/LOST_IP reasons;
  real range-loss and long-running SSH/Weather validation remain pending.
- v0.9.6 was uploaded without erasing NVS; TF logs recorded 85,380 bytes free
  at SSH init and 41,240 at shell-ready. The owner confirmed the connection.

## Product identity

Pocket Deck is a from-scratch, keyboard-first system shell for the **M5Stack
Cardputer Adv**. It is not a Claude Desktop Buddy fork. The archived buddy
firmware may remain in a sibling `../claude-desktop-buddy-cardputer` repository
and must remain untouched unless explicitly selected.

Current apps and services:

- Runtime English / Chinese across every app, Settings, and Quick Settings.
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
  IR, SFTP, SSH tunnels, MQTT, or Home Assistant feature yet.

## Hardware facts

- ESP32-S3FN8 / Stamp-S3A, 8 MB flash, no PSRAM, 240×135 display.
- Partition layout: NVS + one 3 MB app + LittleFS; no OTA slot.
- Keyboard: TCA8418 matrix through M5Cardputer 1.1.1.
- GPS Cap: NMEA UART at 115200, Cardputer RX GPIO15 / TX GPIO13.
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
2. Run `resource-isolation-smoke-test.md`; prioritize MEDIA
   continuity, no Wi-Fi/BT work during playback, deferred-log flush, and bond/profile survival.
3. Stay inside SSH or Weather for several minutes and validate direct reconnect,
   scan backoff, and the new disconnect/LOST_IP reason logs.
4. Run `docs/validation/lora-text-terminal-smoke-test.md` with antenna attached
   and a matching second SX1262/RadioLib endpoint; all RF, shared-SPI, and
   regression rows remain pending until observed.
5. Run the new GPS 4/4 moving, stationary, invalid, and stale-motion rows
   outdoors without sharing precise coordinates.
6. Continue the SSH checklist with two more cold boots, then controls, scrollback, and reconnect.
7. Recheck BLE, Wi-Fi, weather, GPS, heap, and UI responsiveness after the SSH
   worker has been created.
8. Connect a second 2.4 GHz network and confirm both entries appear under Saved
   networks and saved scan rows show `S`.
9. Restart near each network separately and confirm automatic connection.
10. Make both visible and confirm strongest-known selection; then make the first
   candidate fail and confirm fallback.
11. Delete one profile and verify the other survives restart.
12. Recheck BLE typing/range-loss reconnect and confirm Wi-Fi scans occur only
    inside SSH, Weather, or Settings.

Use the matching files in `docs/validation/` to record results. After the device
owner confirms behavior, update this section, commit locally, and push only if
explicitly requested.
