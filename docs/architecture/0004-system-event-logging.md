# ADR-0004: Unified privacy-safe system event logging

**Status:** Accepted

**Date:** 2026-07-30

**Decider:** KEXIN

## Context

Pocket Deck originally stored shared diagnostics under the misleading
`/PocketDeck/ble.log` name and retained only about 1 MB. A MEDIA scan failure
showed why persistent cross-feature evidence matters: the card log proved the
device still ran non-recursive firmware even though newer code existed locally.

The TF card has ample capacity, but the ESP32-S3 has no PSRAM and MP3 playback,
LoRa, and logging share the main task and SPI bus. Logging every loop iteration
or key event would add latency, leak content, and make the logs less useful.

## Decision

- `DiagnosticsService` remains the single event stream; `SdLogService` is its
  persistent sink after TF mounting.
- All applications and services emit bounded lifecycle, state-transition,
  counter, and error events. App switches are logged centrally by `System`.
- The active file is `/PocketDeck/system.log`. It rotates at 4 MB through three
  archives, retaining at most about 16 MB of new-format history.
- `LOG DUMP ALL` and `LOG CLEAR YES` retain compatibility with old
  `ble.log`/`ble-prev.log` files.
- Each event is flushed and closed for power-loss resilience. Events must be
  infrequent enough that this synchronous write policy does not become a loop
  or audio hot path.
- Never log typed keyboard or SSH data, Wi-Fi passwords, pairing codes, exact
  GPS coordinates, MEDIA names/audio, LoRa drafts/payloads, or private keys.

## Consequences

- A single card log now explains which firmware booted, which app was entered,
  and how each subsystem changed state without requiring an attached monitor.
- MEDIA records directory depth, entry counts, playback indexes, and errors but
  not personal filenames.
- GPS emits minute-rate parser health counters without location data.
- Storage use rises from roughly 1 MB to at most roughly 16 MB for current-format
  logs; old legacy files remain until explicitly cleared.
- High-frequency instrumentation still belongs in temporary targeted builds,
  not the persistent event stream.
