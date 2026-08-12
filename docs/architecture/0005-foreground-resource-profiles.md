# ADR 0005: Foreground application resource profiles

Status: accepted

## Context

Pocket Deck previously initialized and continuously updated BLE, Wi-Fi, GPS and
LoRa regardless of the foreground application. Disconnected Wi-Fi scans,
reconnect attempts, UART parsing and synchronous TF diagnostics competed with
the main-loop MP3 producer. They also consumed radio time and power when the
active application could not use them.

## Decision

`System` owns a fixed resource profile for every `AppId`. Applications continue
to request actions through `SystemContext`; they do not directly manage shared
radio lifecycles.

- Launcher: no foreground radio workload.
- Keyboard: BLE HID only.
- SSH: Wi-Fi only.
- GPS: GPS UART/parser only.
- LoRa: SX1262 only.
- MEDIA: TF/audio realtime mode; all radios and GPS parsing are suspended.
- Weather: Wi-Fi and GPS.
- Settings: Wi-Fi and BLE, because it owns scan, network and host management.

Wi-Fi and BLE settings are master permissions. A resource profile may request a
service but cannot override a user-disabled setting. Initialization and activity
are separate: NimBLE stays initialized so bonds survive, the TF card stays
mounted, and services expose inexpensive suspend/resume transitions.

MEDIA and RECORDER are the memory-lifecycle exception defined by ADR-0008:
their large service objects are created only for their foreground profile and
destroyed after the old app completes `onExit`.

MEDIA defers synchronous system-log writes into a small bounded queue. The queue
is flushed only after playback has released its file on application exit.

The transition order is: old app `onExit`, apply the new profile, then new app
`onEnter`. This lets Keyboard send an all-keys-up report, SSH disconnect, and
MEDIA close its decoder before their underlying resources are suspended or
logs are flushed.

## Consequences

- MEDIA no longer competes with Wi-Fi scans, BLE advertising, GPS parsing, LoRa
  receive handling, or per-event TF writes.
- Entering SSH/Weather may include Wi-Fi association latency; entering Keyboard
  may include BLE reconnect latency.
- NTP-backed system time and cached weather remain usable after Wi-Fi suspends.
- GPS UART parsing can be suspended, but this does not claim that the Cap GPS
  module's physical power rail is switched off.
- A Wi-Fi defect can still affect SSH or Weather and must be debugged there; the
  policy isolates it from unrelated apps rather than hiding the defect globally.
