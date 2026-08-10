# Pocket Recorder App Design

Date: 2026-08-10
Target: Pocket Deck on M5Stack Cardputer Adv
Status: Approved for implementation

## Goal

Add a bilingual `RECORDER / 录音机` app that records the Cardputer Adv's built-in
microphone to the already-mounted TF card and plays recordings through the
built-in speaker or 3.5 mm output. Recording must not reintroduce MEDIA-style
audio stutter, corrupt shared TF access, or consume the no-PSRAM device's heap
with the official example's whole-recording buffer.

## Audio and file format

- Directory: `/Recordings`.
- Format: RIFF/WAVE PCM.
- Sample rate: 16,000 Hz.
- Sample size: signed 16-bit.
- Channels: mono.
- Data rate: 32,000 bytes per second, approximately 115 MB per hour.

If synchronized wall-clock time is valid, use
`REC_YYYYMMDD_HHMMSS.WAV`. Otherwise choose the first unused sequential name
`REC_NNNN.WAV`. File names are user data and are never translated or logged.

## User experience

The app has two pages.

### Record page

- Simplified live waveform and level meter.
- Elapsed time, current file size, and cached TF free space.
- Enter starts recording; Enter again stops, finalizes, and saves.
- A clear red recording state remains visible for the whole recording.

### Files page

- Tab changes between Record and Files.
- Fn+Up/Fn+Down select among at most 64 fixed-capacity entries.
- Enter starts or stops playback of a compatible recording.
- `d` opens a separate delete-confirmation view.
- Enter confirms deletion; Backspace cancels the confirmation.
- Backspace or G0 returns Home when no confirmation is open.

The list shows only regular `.WAV` files in `/Recordings`, sorted newest-name
first. This release plays only the format it records; arbitrary compressed or
multichannel WAV variants produce `UNSUPPORTED WAV / 不支持的 WAV`.

## Streaming architecture

Add a `RecorderService`, a portable recorder model, and `RecorderApp`.

Recording uses two fixed 1,024-sample PCM buffers. Total application PCM capture
storage is 4,096 bytes. M5Unified fills one buffer while the main system task
writes a completed buffer to the already-mounted `SD` instance. No `String`,
`std::vector`, whole-recording allocation, second SD mount, or recorder worker
task is introduced.

The service writes a placeholder WAV header when recording starts, tracks the
number of successfully written PCM bytes, and checkpoints the header and flushes
the file every two seconds. Stop, app exit, or recoverable error finalizes the
header and closes the file. A sudden power loss can lose the final in-flight
buffer but leaves the most recently checkpointed WAV length.

Playback parses the RIFF/WAVE header, validates PCM mono/16-bit/16 kHz, and
streams fixed-size chunks to M5Unified Speaker. Inserting a 3.5 mm output device
uses the Cardputer Adv hardware's normal audio routing.

## Audio ownership

The Cardputer Adv microphone and speaker cannot be active together through the
current M5Unified audio path.

- Starting recording stops playback, ends Speaker, then starts Mic.
- Stopping recording waits for queued capture completion, ends Mic, finalizes
  and closes the file, begins Speaker, and restores the persisted volume.
- Starting playback ensures Mic is ended and Speaker is active.
- Leaving the app always closes the file, ends Mic, stops playback, restores
  Speaker, and restores volume before another app is activated.

## Resource and TF ownership

Add `RuntimeResource::RecorderRealtime`; only `AppId::Recorder` requests it.
Like MEDIA realtime mode, it suspends BLE, Wi-Fi, GPS, and LoRa work and defers
TF diagnostic writes before the app opens a recording or playback file.

The recorder uses the SD instance already owned and mounted by `SdLogService`.
It never mounts, unmounts, formats, or repairs a card. `System::openApp` already
calls the current app's exit path before applying the next profile, so recorder
files close before deferred diagnostics flush.

## Failure handling

- No mounted card: show `NO TF CARD / 未检测到 TF 卡`; do not mount or format.
- Directory creation failure: remain responsive and show a storage error.
- Mic start failure: restore Speaker and do not create an active recording.
- File open or short write: stop capture, finalize bytes written if possible,
  close the file, restore audio, and show an error.
- Card full: same safe stop path; never loop on repeated writes.
- Unsupported or damaged WAV: close the file and return to the list.
- App exit during record/playback: stop and clean up synchronously before Home.

## Diagnostics and privacy

Log only lifecycle, categorical state transitions, duration/byte totals, and
categorical failures. Never log filenames, audio samples, waveform values,
recorded content, or directory listings. TF diagnostics remain deferred while
recorder realtime mode is active and flush after all recorder files close.

## Validation

- Native tests cover WAV header fields and checkpoint sizes, sequential naming,
  format validation, list capacity/sorting, delete confirmation, state cleanup,
  and recorder resource policy.
- Full build verifies static RAM, flash, and the existing SSH memory budget.
- Hardware checks cover 30-second and multi-minute recordings, playback,
  speaker/AUX routing, no-card behavior, app exit while recording, deletion,
  power interruption tolerance, and card-full/short-write behavior where safe.
- Regression checks cover MEDIA playback, restored system sounds, TF logging,
  LoRa shared SPI, BLE, Wi-Fi, GPS, IR, UI responsiveness, and SSH public-key
  authentication after Recorder has been used.

## Non-goals

- MP3/AAC recording, editing, trimming, transcription, cloud upload, or speech
  recognition.
- Background or always-listening recording.
- More than 64 displayed recordings or recursive recording directories.
