# ADR-0008: Allocate heavy media services only while foreground

**Status:** Accepted

**Date:** 2026-08-10

## Context

The Cardputer Adv has no PSRAM. `MediaService` retained an 8,920-byte current-
folder library and `RecorderService` retained 13,616 bytes of file metadata and
audio buffers inside the global `System` object even when neither app was open.
Foreground resource profiles stopped their work but could not reclaim that
static RAM.

After RECORDER integration, Weather repeatedly reached DNS and TCP/443 but its
TLS handshake failed with `esp-sha: Failed to allocate buf memory`. The linked
ESP-IDF 4.4.7 TLS stack uses two 16 KiB record buffers and its hardware SHA path
also needs a small internal DMA-capable block. The inactive services left too
little suitable memory for that allocation.

## Decision

- `System` owns MEDIA and RECORDER through `AppScopedService<T>` instead of as
  permanent by-value members.
- Applying the MEDIA or RECORDER foreground profile creates the corresponding
  service before the new app's `onEnter`. Allocation failure leaves its
  `SystemContext` pointer null and emits one categorical diagnostic.
- The old app still receives `onExit` first, so playback, recording, TF files,
  Mic, and Speaker are released normally. Applying the next profile then
  destroys the inactive service and returns all of its memory to the heap.
- Re-entering either app creates a fresh service and scans its TF directory
  again. No filenames, audio data, or other user content persist in RAM across
  app exits.
- Radio, GPS, display, settings, diagnostics, and storage-mount services remain
  persistent because they retain cross-app state or own shared hardware
  lifecycles. This decision does not authorize arbitrary heap-backed state.

## Consequences

- The global `System` object falls from 38,552 to 16,024 bytes and firmware
  static RAM falls by 22,528 bytes. A physical Weather request subsequently
  completed with `Weather state: READY`.
- MEDIA and RECORDER pay one bounded allocation and directory rescan on app
  entry, when their profiles have already suspended Wi-Fi, BLE, GPS, and LoRa.
- Their pointers are intentionally unavailable outside the owning foreground
  app. New callers must handle a null service rather than assuming permanent
  residency.
- Native lifecycle coverage verifies that repeated activation constructs once
  and deactivation destroys the service. Physical MEDIA playback and RECORDER
  capture/playback still require regression checks after this ownership change.
