# Pocket Deck agent guide

These instructions apply to the entire `pocket-deck` repository.

## Scope

- Pocket Deck is standalone firmware for the **M5Stack Cardputer Adv** only.
- It is not based on Claude Desktop Buddy and does not implement Claude/NUS
  integration. Do not import code or assumptions from sibling repositories.
- Preserve the sibling `../claude-desktop-buddy-cardputer/` repository when it
  exists; it is fallback firmware and is outside this repository.
- Read `MEMORY.md` before substantial work and update its current-state section
  after a completed feature or important debugging result.

## Build and validation

Run commands from this repository root:

```bash
scripts/test-native.sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

- Use the `cardputer-adv` environment and the PlatformIO packages under
  `~/.platformio`. Do not introduce an Arduino IDE workflow.
- Every code change must pass `scripts/test-native.sh`, `git diff --check`, and a
  complete `pio run -e cardputer-adv` before handoff.
- A successful build is not hardware validation. Use the matching checklist in
  `docs/validation/` and report what was and was not tested on-device.
- Ordinary uploads should work after power-off and USB connection. Use the G0
  download-mode sequence only when the serial port is absent or upload fails.
- `uploadfs` is currently unnecessary because the firmware has no filesystem
  assets.

## Hardware constraints

- Target: Stamp-S3A / ESP32-S3FN8, 8 MB flash, about 320 KB SRAM, no PSRAM.
- Display: 240×135. Keep labels compact and test clipping on the actual screen.
- BLE and Wi-Fi share the 2.4 GHz radio. Keep scans and network work asynchronous
  so HID input and rendering remain responsive.
- Optional Cap LoRa-1262 GPS uses UART RX GPIO15, TX GPIO13, 115200 baud.
- TF logging shares SPI pins with the Cap. Keep the Cap chip-select inactive and
  avoid long-lived open files.
- MEDIA is the one intentional exception: it may keep one read-only MP3 file
  open only while foreground playback is active. Audio/SD work stays on the main
  task and the file must close before mount, format, or app exit.
- MEDIA browses at most four folder levels and stores at most 64 entries from
  the current folder. Folder changes close playback first; Chinese names use
  the built-in M5GFX font and must not introduce a LittleFS dependency.
- Native USB re-enumerates on reset; a monitor attached afterward may miss early
  serial output. Persistent TF logs are the reliable boot-history source.

## Architecture boundaries

- `src/core/`: state, policies, routing, fixed-size data models; keep it portable
  enough for native tests.
- `src/drivers/`: direct Cardputer hardware access.
- `src/services/`: BLE, Wi-Fi, GPS, weather, NVS, TF logging.
- `src/apps/`: app-specific input and rendering.
- `src/ui/`: shared status and Quick Settings components.
- `System` owns service/app lifecycle and command dispatch. Apps request actions
  through `SystemContext`; they should not manipulate radio or storage drivers.
- Prefer fixed-capacity arrays and explicit state machines over unbounded heap
  allocation or blocking loops.

## Security and privacy invariants

- BLE HID accepts one bonded host and sends reports only from the foreground
  Keyboard app over an authenticated, encrypted link.
- Never log typed characters, HID reports, Wi-Fi passwords, pairing passkeys, or
  precise GPS coordinates. MEDIA also never logs track filenames or audio data.
- Wi-Fi owns up to eight profiles in the `pocketwifi` NVS namespace. Arduino
  persistence and automatic reconnect stay disabled; `WifiService` controls
  scan, selection, fallback, and retry.
- Save Wi-Fi credentials only after `WL_CONNECTED`. Factory reset and per-network
  deletion must also prevent migration of a stale legacy ESP32 station record.
- Destructive UI actions require a separate confirmation page.

## Documentation ownership

- `README.md`: concise public user/build guide; no session history.
- `MEMORY.md`: current project handoff, validated state, blockers, and near-term
  work; keep it under 200 lines.
- `docs/architecture/`: durable decisions and trade-offs; add an ADR when a
  subsystem ownership rule changes.
- `docs/validation/`: reusable real-hardware checklists; do not mark a row passed
  from compilation alone.
- Link to an existing document instead of duplicating long explanations.

## Git and external actions

- Preserve unrelated user changes and stage explicit files only.
- Local commits may be made when requested. Push to GitHub only after the user
  explicitly asks to push or synchronize.
- Do not erase the device, clear NVS/BLE bonds, format TF storage, or publish a
  release without explicit authorization.
