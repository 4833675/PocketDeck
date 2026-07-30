# MEDIA MP3 Player Design

**Date:** 2026-07-30

**Status:** Approved for implementation

## Goal

Add a foreground-only `MEDIA` app that plays MP3 files from the Cardputer Adv
TF card through the built-in speaker or 3.5 mm audio output. The first version
must remain responsive alongside Pocket Deck's existing BLE, Wi-Fi, GPS, LoRa,
SSH, diagnostics, and TF logging services on a device with no PSRAM.

## Scope

- Scan the non-recursive `/Music` directory for `.mp3` files, case-insensitively.
- Keep at most 32 tracks in a fixed-capacity, case-insensitively sorted library.
- Show a compact scrollable library with the selected and currently loaded track.
- Play through `M5Cardputer.Speaker` using the same ESP8266Audio adapter pattern
  as M5Stack's official M5Unified MP3 example. Version 2.2.0 is pinned because
  later releases require the newer ESP32 I2S API absent from this toolchain.
- Show playback state, elapsed active-play time, and byte-position progress.
- Automatically advance to the next track when a file finishes.
- Reuse the system volume and persist volume changes through the existing
  settings path.
- Stop playback, close the file, and release decoder memory when MEDIA exits.

The first version excludes ID3 metadata, cover art, waveform/FFT graphics,
seeking, playlists, recursive folders, shuffle/repeat modes, background
playback, network streams, WAV/AAC/FLAC, GIF, MJPEG, and other video formats.

## Interaction

- `Fn+Up` / `Fn+Down`: move the library selection.
- `Enter`: start the selected track; if it is already loaded, toggle pause/resume.
- `Fn+Left` / `Fn+Right`: load and play the previous/next track with wraparound.
- `-` / `=`: decrease/increase volume by 5 percentage points and persist it.
- `Backspace` or G0 tap: stop playback and return Home.

Opening MEDIA scans the card. If playback is already stopped, rescanning is
allowed with `Tab`; the scan never runs while a decoder owns an open track.

## Architecture

### Portable core

`src/core/media_data.*` owns fixed-size track records, filtering, sorting,
selection, wraparound, and display-safe filename extraction. It has no Arduino
dependency and is covered by native tests.

`src/apps/media/media_app_model.*` translates input into explicit effects such
as select, play/toggle, skip, rescan, volume adjustment, and Home. It contains no
SD or audio calls and is covered by native tests.

### Hardware service

`src/services/media_service.*` owns SD scanning and MP3 playback. It uses the
already-mounted global `SD` instance; it never calls `SD.begin`, `SD.end`, or
formats the card. It uses `AudioFileSourceSD`, `AudioGeneratorMP3`, and a small
M5 speaker output adapter based on M5Stack's official example.

Decoder/source/output objects are allocated only when a track starts and are
destroyed on stop or app exit. Allocation and decoder-start failures produce a
visible error instead of restarting the device. Track names and playback
content are never written to diagnostics.

### System integration

`System` owns one `MediaService` and one `MediaApp`, exposes the service through
`SystemContext`, updates the decoder from the main task, and routes requested
volume changes through `Board`, `SystemSettings`, and `SettingsStore`.

MEDIA is added to the launcher between LORA and WEATHER. The firmware version
becomes `0.6.0`.

## Shared-resource rules

- MEDIA does not mount or unmount TF storage and does not keep directory handles
  open after scanning.
- The MP3 file remains open only during foreground playback.
- SD, diagnostics, and SX1262 operations remain serialized through the existing
  main-task update loop; no audio worker accesses SPI from another task.
- The SX1262 NSS and TF CS behavior established by the existing services is not
  changed.
- Log appends may occur between decoder updates because each append opens,
  flushes, and closes its file. Playback must tolerate this short interruption.
- Starting MEDIA after an SSH session may encounter a fragmented heap. All
  player allocations are checked and failure is reported as `OUT OF MEMORY`.

## Error states

- TF unavailable: `NO TF CARD`, with a pointer to Settings > System > TF logs.
- `/Music` absent: create it when possible and show `COPY MP3 TO /Music`.
- No MP3 files: `NO MP3 FILES`.
- Library limit reached: play the first 32 sorted tracks and show `32+`.
- Open/decode failure: preserve the library, stop cleanly, and show a short
  filename-independent error.
- Unexpected end or decoder failure: close resources; normal end advances,
  decoder error remains stopped and visible.

## Validation

Automated validation covers MP3 extension matching, deterministic sorting,
capacity limits, selection wraparound, app effects, progress clamping, and
elapsed-time pause behavior. Every code change must pass:

```bash
scripts/test-native.sh
git diff --check
pio run -e cardputer-adv
```

Hardware validation uses a FAT-formatted TF card with short test files under
`/Music` and checks scan, playback, pause/resume, auto-next, volume persistence,
speaker/AUX switching, exit cleanup, missing/bad files, UI responsiveness,
TF-log coexistence, LoRa regression, and the low-memory path after SSH use.

## Alternatives considered

1. **ESP8266Audio plus M5 speaker adapter (selected):** follows M5Stack's
   official MP3 example and preserves the Cardputer Adv codec configuration.
2. **WAV-only playback:** simpler and lower risk, but does not satisfy the MP3
   request and consumes much more TF storage.
3. **Custom Helix/I2S pipeline:** gives tighter control but duplicates working
   decoder and M5 speaker integration code without first-version benefit.
