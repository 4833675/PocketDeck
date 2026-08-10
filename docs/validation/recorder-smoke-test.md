# Pocket Deck RECORDER Hardware Smoke Test

Target: M5Stack Cardputer Adv running the final integrated firmware. RECORDER /
录音机 writes and plays only its own 16 kHz, mono, signed-16-bit PCM WAV format
in `/Recordings`. It uses the existing TF mount and never mounts, formats,
repairs, or erases the card.

Do not treat compilation, native tests, a visible screen, or a created directory
as evidence of microphone, speaker, AUX, card, or recovery behavior. Do not
erase flash, clear NVS/BLE bonds, format TF, or include recording names, audio,
typed text, credentials, or precise coordinates in notes.

## Automated preflight

These results validate the final integrated source/build only. They do not prove
physical recording or playback.

| ID | Command | Expected | Result | Evidence |
|---|---|---|---|---|
| REC-BUILD-01 | `scripts/test-native.sh` | Native checks pass | Passed | `PASS: 1601 checks`; source only. |
| REC-BUILD-02 | `git diff --check` | No whitespace errors | Passed | Final documentation/source tree; source only. |
| REC-BUILD-03 | `pio run -e cardputer-adv` | Cardputer Adv target builds | Passed | RAM `104728 / 327680` bytes (32.0%); flash `2134157 / 3145728` bytes (67.8%); source only. |

## Physical Cardputer Adv checks

Keep every row `Pending` until it has been observed on the physical device.
Where a failure case is destructive or difficult to reproduce safely, retain
`Pending` rather than damaging a card or manufacturing an invalid result.

| ID | Test | Procedure and expected result | Result | Notes |
|---|---|---|---|---|
| REC-01 | Entry and idle UI | Open RECORDER in English and Chinese; Record/Files navigate while idle; elapsed, bytes, cached free space, waveform/level region, and Home actions fit and remain responsive. | Pending | |
| REC-02 | 30-second capture | Start from Record, capture for at least 30 seconds, then stop. A red recording state remains visible; level/waveform, elapsed, and bytes advance. | Pending | |
| REC-03 | Multi-minute capture | Record for several minutes, then stop. UI stays responsive without reset, obvious stutter, or runaway resource use. | Pending | |
| REC-04 | Finalized WAV | After a normal stop, select the resulting entry and play it. The finalized header is accepted and length/duration are plausible. Do not put its name or content in notes. | Pending | |
| REC-05 | Speaker and AUX | Play a recording through the built-in speaker, then safely test 3.5 mm output if available. Audio routes normally and stops on Enter/Home. | Pending | |
| REC-06 | Files/list limit | Confirm recent recordings appear newest-name first, Fn+Up/Down wraps safely, and the UI remains usable with many entries; no more than 64 are displayed. | Pending | Do not record names. |
| REC-07 | Delete confirmation | On Files, press `d`, cancel with Backspace, reopen confirmation, then Enter to delete the selected entry. Only confirmed deletion occurs. | Pending | Do not identify the entry in notes. |
| REC-08 | No card | With no mounted card, open RECORDER. It reports no-card and remains responsive; it does not mount, format, or repair storage. | Pending | Safely reproducible only. |
| REC-09 | Storage create/open | With a mounted writable card, enter RECORDER and begin/stop once. `/Recordings` is usable without a second mount or filesystem repair. | Pending | Do not disclose path contents. |
| REC-10 | Mic failure category | If safely reproducible without hardware modification, verify a microphone-start/queue/timeout failure returns audio to a usable state and shows a categorical error. | Pending | Do not induce a damaging fault. |
| REC-11 | Short write/card full | Only with a disposable test card or safely controlled full-card condition, verify short-write/card-full stops capture, finalizes written bytes when possible, and remains responsive. | Pending | Do not intentionally damage user data. |
| REC-12 | Exit during record | Start capture, then use Backspace or G0. Before Home appears, Mic stops, the file is finalized/closed, normal sound returns, and the resulting WAV is handled safely. | Pending | |
| REC-13 | Exit during playback | Start playback, then leave with Backspace or G0. Playback stops, the file closes, Speaker returns to normal, and Home remains responsive. | Pending | |
| REC-14 | Power interruption recovery | On a disposable card only, interrupt power after recording exceeds a checkpoint interval; after restart, inspect only whether the prior WAV is safely handled to its last checkpointed length. | Pending | Do not use a card with needed data. |
| REC-15 | MEDIA restoration | Exit RECORDER, then play/pause/stop a known MP3 in MEDIA. MP3 audio and controls remain responsive. | Pending | Do not record track names. |
| REC-16 | System-sound restoration | Change a non-destructive UI selection after record and after playback. Normal system feedback and persisted volume behavior return. | Pending | |
| REC-17 | TF log resumption | After Recorder has fully exited, inspect only categorical diagnostics via the approved log workflow. Deferred TF logging resumes after recorder files are closed. | Pending | Do not expose audio, names, or private content. |
| REC-18 | Shared-SPI LoRa | With Cap antenna attached, open LORA after exiting RECORDER; do not transmit unless authorized. LoRa reaches normal state/error without reset or TF corruption. | Pending | |
| REC-19 | BLE/Wi-Fi/GPS/IR | After Recorder use, test the bonded Keyboard, an existing Wi-Fi profile, GPS UI, and REMOTE navigation. No bond/profile loss, hang, or unexpected recording state occurs. | Pending | Do not type/log private data or coordinates. |
| REC-20 | SSH authentication | After Recorder use, connect to an already-configured host with public-key auth. Terminal remains usable and no terminal content enters diagnostics. | Pending | Do not disclose host details or terminal text. |

## Result summary

| Area | Status | Follow-up |
|---|---|---|
| Automated preflight | Passed (source/build only) | Does not establish on-device audio or card behavior. |
| Record, finalization, and playback | Pending | REC-01 through REC-05 require direct observation. |
| Error and recovery paths | Pending | REC-08 through REC-14 only when safely reproducible. |
| Resource, diagnostics, and regressions | Pending | REC-15 through REC-20 require direct observation. |
