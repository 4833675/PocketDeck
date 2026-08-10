# Sony IR Remote App Design

Date: 2026-08-10
Target: Pocket Deck on M5Stack Cardputer Adv
Television: Sony KD-65X9100H
Status: Approved for implementation

## Goal

Add a bilingual `REMOTE / 遥控器` app for the Cardputer Adv's built-in GPIO44
infrared emitter. Reimplement the Sony SIRC behavior already proven useful in
the archived buddy firmware, without adding a source or runtime dependency on
that repository.

The archived handoff used the model label KD-65X9000H. The device owner has now
explicitly identified the television as KD-65X9100H, so the new UI and
documentation use KD-65X9100H while retaining the working command baseline.

## Command map

Each press sends three Sony SIRC frames: one initial frame plus two repeats.

| Pocket Deck input | Television action | Device | Command | Frame |
|---|---|---:|---:|---:|
| Fn+Up | D-pad up | 151 | `0x4F` | 15-bit |
| Fn+Down | D-pad down | 151 | `0x50` | 15-bit |
| Fn+Left | D-pad left | 151 | `0x4D` | 15-bit |
| Fn+Right | D-pad right | 151 | `0x4E` | 15-bit |
| Enter | OK | 151 | `0x4A` | 15-bit |
| Backspace | Back | 151 | `0x23` | 15-bit |
| Backtick | Return | 1 | `0x63` | 12-bit |
| `p` | Power | 1 | `0x15` | 12-bit |
| `h` | Home | 1 | `0x60` | 12-bit |
| `i` | Input | 1 | `0x25` | 12-bit |
| `m` | Mute | 1 | `0x14` | 12-bit |
| `-` | Volume down | 1 | `0x13` | 12-bit |
| `=` | Volume up | 1 | `0x12` | 12-bit |
| G0 | Return to Pocket Deck Home | - | - | - |

Backspace belongs to the television while this app is foreground. G0 is always
available as the unambiguous Pocket Deck exit, so the user cannot be trapped in
the remote app.

## Architecture

Add an `IrService` that owns Arduino-IRremote initialization and transmission.
The implementation is send-only, defines `DISABLE_CODE_FOR_RECEIVER`, and uses
GPIO44. `IRremote.hpp` is included from one implementation unit only.

Add `RuntimeResource::Ir`; only `AppId::Remote` requests it. Entering the app
starts the sender and leaving it calls the sender's shutdown path. The service
has no background polling.

The app uses text input so letter and punctuation commands remain available,
while the existing Fn navigation mapping supplies the four D-pad actions. The
service accepts semantic commands rather than raw UI keys, keeping the Sony
protocol map out of input routing and rendering code.

## UI

The screen shows:

- `Sony KD-65X9100H`;
- compact bilingual control hints;
- the most recently requested semantic action;
- `SENT / 已发送` after a successful library call, or an explicit local error.

The built-in hardware is transmit-only. The app must not claim that the TV
received a command, and it does not offer IR learning.

## Diagnostics and privacy

Log app/service lifecycle, initialization errors, and semantic action names
only. Do not log timestamps for every repeat frame or create a hot-loop log.
Sony device IDs and command values are protocol constants and may remain in
source and documentation.

## Validation

- Native tests cover the semantic command table, 12/15-bit device selection,
  repeat count, input mapping, and IR resource profile.
- Full build verifies the IRremote dependency does not break timer, audio, BLE,
  or display compilation.
- Hardware testing on the KD-65X9100H checks every command individually,
  repeated volume presses, app exit through G0, and subsequent MEDIA, BLE, GPS,
  LoRa, Wi-Fi, and SSH operation.
- A displayed `SENT` result means only that transmission was requested locally.

## Non-goals

- Infrared receive or learning.
- Other television brands, configurable code databases, macros, or long-press
  repeat in the first release.
