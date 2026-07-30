# Pocket Deck LoRa Text Terminal and GPS Motion Page Design

Status: Approved
Date: 2026-07-30  
Target: M5Stack Cardputer Adv with Cap LoRa-1262 (SKU U214)

## Objective

Add a raw LoRa point-to-point text terminal compatible with ordinary
SX1262/RadioLib peers, while preserving the existing TF diagnostics, GPS,
Bluetooth keyboard, Wi-Fi, weather, and SSH features. Also add a fourth GPS page
dedicated to speed and course.

This is raw LoRa, not LoRaWAN. The first version uses one fixed radio profile and
does not include encryption, acknowledgements, retransmission, addressing, or a
Pocket Deck-specific packet header.

## Selected product approach

The selected approach is one keyboard-first message screen:

- Type one printable-ASCII message in a fixed 120-byte draft buffer.
- Press Enter to transmit the exact draft bytes as one LoRa packet.
- Show recent sent and received messages together, marked `TX>` and `RX>`.
- Show the latest received packet's RSSI and SNR without logging its payload.
- Keep the radio in receive mode whenever it is not transmitting.
- Use `Fn+Backtick` to leave the app and Backspace to edit the draft.

Two alternatives were considered and rejected:

1. Separate Send and Receive pages add navigation but no capability on a
   240x135 keyboard device.
2. A beacon/packet-monitor-only app is easier to test with one unit, but does not
   satisfy the requested general text communication use case.

## Fixed interoperability profile

Use RadioLib 7.7.1 and initialize the SX1262 with the M5Stack tutorial profile:

| Parameter | Value |
|---|---:|
| Carrier frequency | 868.0 MHz |
| Bandwidth | 125.0 kHz |
| Spreading factor | SF12 |
| Coding rate | 4/5 (`5` in RadioLib) |
| Sync word | `0x34` |
| Transmit power | +22 dBm |
| Preamble | 20 symbols |
| TCXO voltage | 3.0 V |
| Current limit | 140 mA |

Every generic peer must use the same modulation parameters. The payload itself
contains only the text bytes, so a matching RadioLib receiver can read it without
understanding any Pocket Deck protocol.

## Hardware integration

The Cap uses these Cardputer Adv connections:

- SX1262 NSS G5, IRQ/DIO1 G4, reset G3, busy G6.
- Shared SPI SCK G40, MOSI G14, MISO G39.
- Antenna switch through PI4IOE5V6408 at I2C address `0x43`, P0 driven high.
- GPS remains on UART RX G15 and TX G13 at 115200 baud.

The SX1262 and TF card share one SPI bus. TF uses CS G12 and LoRa uses NSS G5.
Initialization must keep both chip-select lines high except during their own
transactions. All RadioLib calls occur from `System::update()`; the DIO1 ISR only
sets a volatile flag. Existing TF writes also run on the main system task, so the
first version has no concurrent SPI transactions and needs no separate mutex.
Any future worker-thread SPI user must introduce explicit bus arbitration.

The product warning remains binding: an antenna must be attached before LoRa is
initialized or transmitted.

## Architecture

### Portable core model

Add a fixed-capacity LoRa model under `src/core/`:

- Radio states: unavailable, initializing, listening, transmitting, and error.
- Six recent message records with direction, length, and printable text.
- One 121-byte draft array, including the terminator.
- Counters for received, sent, CRC failures, dropped packets, and last RadioLib
  status code.
- Latest valid RSSI and SNR.

The model owns editing, send eligibility, bounded history, and sanitization.
Incoming bytes outside printable ASCII are represented as `.` on screen. No
dynamic `String`, unbounded container, or payload logging is permitted.

### LoRa service

Add `LoRaService` under `src/services/` using `SX1262` and RadioLib:

1. Initialize lazily when the LoRa app is opened for the first time. This keeps a
   missing Cap from affecting normal boot.
2. Enable the Cap antenna switch before `radio.begin()`.
3. Register one DIO1 callback and enter non-blocking `startReceive()`.
4. On RX-done, read at most 120 bytes, capture RSSI/SNR, publish one bounded
   message, and immediately restart receive mode.
5. On send, switch from receive to non-blocking `startTransmit()`; after TX-done,
   publish the sent record and return to receive mode.
6. Treat CRC mismatch as a counted packet loss and resume listening. For another
   operation failure, expose the RadioLib code, attempt one clean standby/restart,
   and leave a persistent error state if recovery fails.

After first initialization the service continues receiving in the background,
even when another app is foreground. It stores only its six-message RAM history;
there is no notification, NVS persistence, or payload entry in serial/TF logs.

### LoRa app

Add `AppId::LoRa`, a launcher card, and `src/apps/lora/`.

The app uses `InputMode::Text`. Its layout is:

- Status bar title `LORA`.
- Compact profile/state row such as `868.0  SF12  LISTENING`.
- Recent message area, newest visible records at the bottom, with `RX>`/`TX>`.
- Latest RX quality as `RSSI -xxx  SNR +x.x` when available.
- Bottom draft field and a compact `ENTER SEND` hint.

Enter sends only a non-empty draft while the radio is listening. Additional
Enter presses while transmitting do not duplicate the packet. Leaving the app
clears the unsent draft but retains in-memory history. Payloads longer than 120
bytes are impossible through the editor and oversized received packets are
discarded and counted.

### System integration

`System` owns `LoRaService` and `LoRaApp`, calls `LoRaService::update()` once per
main loop, and exposes the service through `SystemContext`. The launcher expands
from five to six apps. LoRa initialization failure must not block GPS parsing,
rendering, BLE, Wi-Fi, SSH, or TF logging.

## GPS page 4/4

The current parser already exposes `speedKph`, `courseDegrees`, and compass-point
conversion. Change GPS page cycling from three to four pages and add a body that
contains only:

- A large numeric speed with `KM/H`.
- A large course in degrees and its compass point, for example `082°  E`.

The status bar reads `GPS 4/4`. If the relevant value is invalid or GPS data is
stale, show `--` instead of retaining a misleading motion value. Existing pages
1-3 otherwise remain unchanged.

## Privacy and failure behavior

- Never log transmitted drafts, received payloads, or message history.
- Diagnostics may log radio state transitions, integer error codes, packet
  lengths, and aggregate counters only.
- A missing/undetected Cap displays `RADIO NOT FOUND`; the rest of Pocket Deck
  remains usable.
- A busy/transmitting radio rejects a new send visibly and never queues a hidden
  duplicate.
- LoRa is half-duplex; packets arriving during local transmission may be missed.
- No message-delivery guarantee is implied without acknowledgements.

## Verification

Native tests cover:

- Draft append, erase, 120-byte limit, and empty-send rejection.
- Exact raw payload extraction with no custom header.
- Six-record history eviction and direction labels.
- Incoming non-printable sanitization and oversized-packet rejection.
- Radio state transitions for initialize, RX, TX, CRC error, and recovery.
- Launcher wraparound after adding the sixth app.
- GPS page cycling across four pages.

Build checks remain `scripts/test-native.sh`, `git diff --check`, and
`pio run -e cardputer-adv`.

Hardware validation uses two matching RadioLib/SX1262 endpoints and verifies:

- Cap detection and antenna-switch initialization.
- Bidirectional ASCII payload equality up to 120 bytes.
- RSSI/SNR display, repeated RX re-arming, CRC/error recovery, and no duplicate
  sends.
- TF logging while packets are received and transmitted, including a restart
  with the TF card inserted.
- GPS page 4 speed/course behavior while moving and while fix/data becomes stale.
- BLE, Wi-Fi/weather, and SSH remain usable after LoRa initialization.

No GitHub push occurs until device testing is accepted explicitly.

## References

- [M5Stack Cap LoRa-1262 product documentation](https://docs.m5stack.com/zh_CN/cap/Cap_LoRa-1262)
- [M5Stack Cap LoRa-1262 Arduino tutorial](https://docs.m5stack.com/zh_CN/arduino/projects/cap/cap_lora868)
- [RadioLib SX1262 API](https://jgromes.github.io/RadioLib/class_s_x1262.html)
