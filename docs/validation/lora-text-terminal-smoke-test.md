# Pocket Deck LoRa Text Terminal Hardware Smoke Test

Target: M5Stack Cardputer Adv with M5Stack Cap LoRa-1262 and RadioLib 7.7.1.

**Attach the Cap antenna before opening LORA, initializing the radio, or
transmitting. Do not operate or transmit without an antenna.**

This feature is raw LoRa P2P, not LoRaWAN. It has no encryption, addressing,
ACK, retry, delivery guarantee, or hidden transmit queue; it is half-duplex, so
a packet can be missed during local transmission. Interoperability requires a
second powered SX1262 endpoint using RadioLib and the exact profile below. A
build or native test is not evidence of Cap detection, RF operation, or
two-endpoint delivery.

## Fixed interoperability profile

| Parameter | Required value |
|---|---:|
| Radio library | RadioLib 7.7.1 |
| Mode | Raw LoRa P2P (not LoRaWAN) |
| Carrier frequency | 868.0 MHz |
| Bandwidth | 125.0 kHz |
| Spreading factor | SF12 |
| Coding rate | 4/5 (`5` in RadioLib) |
| Sync word | `0x34` |
| Transmit power | +22 dBm |
| Preamble | 20 symbols |
| TCXO voltage | 3.0 V |
| Current limit | 140 mA |

The second endpoint must apply every modulation value above. Confirm that use
of 868.0 MHz and +22 dBm is permitted at the test location before transmitting.

## Evidence record

| Field | Value |
|---|---|
| Date / tester | Pending |
| Firmware commit | Pending |
| Cardputer Adv / Cap | Pending |
| Second SX1262 endpoint and RadioLib version | Pending |
| Antennas attached before radio initialization | Pending |
| Test location authorization | Pending |

## Automated preflight

| Check | Command | Expected evidence | Result | Notes |
|---|---|---|---|---|
| Native model tests | `scripts/test-native.sh` | LoRa payload, state, history, and GPS navigation checks pass | Pass | 771 checks; native only, not RF evidence |
| Firmware build | `pio run -e cardputer-adv` | RadioLib 7.7.1 compiles for Cardputer Adv | Pass | Clean build; not hardware evidence |
| Whitespace | `git diff --check` | No whitespace errors | Pass | Task 5 validation run |

## Cap, radio, and two-endpoint checks

Keep every hardware row `Pending` until it is physically observed. Do not record
payload text in this file, serial output, or TF logs. For equality tests, record
only direction, byte length, and a non-reversible test-run identifier; observe
byte-for-byte equality on the receiving endpoint without copying the payload.

| ID | Test | Procedure | Expected | Result | Notes |
|---|---|---|---|---|---|
| LORA-01 | Antenna and Cap | With the antenna attached, boot normally and open LORA. | Cap antenna switch enables before radio init; state reaches `LISTENING`, or a clear radio error is shown without blocking the system. | Pending | |
| LORA-02 | Cap absent/recovery | With no RF test, start from normal boot and verify a missing or unavailable Cap state only if safe to do so. | `RADIO NOT FOUND`/error is visible; Home and other apps remain usable. | Pending | Do not initialize or transmit without antenna. |
| LORA-03 | Pocket Deck to peer, short | Send one tester-chosen printable-ASCII draft below 120 bytes to the matching peer. | Peer receives exactly the same raw bytes, with no Pocket Deck header. | Pending | Record length only. |
| LORA-04 | Peer to Pocket Deck, short | Send one tester-chosen printable-ASCII payload below 120 bytes from the peer. | Pocket Deck shows one `RX>` record with byte-for-byte matching printable content. | Pending | Record length only. |
| LORA-05 | 120-byte boundary, both directions | Repeat LORA-03 and LORA-04 at exactly 120 printable ASCII bytes. | Both directions preserve exact raw bytes; no truncation or hidden header. | Pending | Record lengths only. |
| LORA-06 | Oversize receive | From the peer, attempt a payload longer than 120 bytes. | Pocket Deck drops it, records no partial message, and remains able to receive. | Pending | Do not record payload. |
| LORA-07 | RSSI/SNR | Receive a valid peer packet after LORA-04 or LORA-05. | Latest RX RSSI and SNR appear and are plausible for the setup. | Pending | Record measurements, not text. |
| LORA-08 | RX re-arm | Send several separated peer packets while Pocket Deck is otherwise idle. | Each valid packet appears once; receive mode re-arms after every RX completion. | Pending | Record count only. |
| LORA-09 | Duplicate-send guard | Hold or repeat Enter while one non-empty draft is transmitting. | Only one packet is observed by the peer; Pocket Deck visibly rejects the busy send and queues nothing. | Pending | Record packet count only. |
| LORA-10 | CRC/error recovery | Induce a receiver CRC/error condition using the peer or controlled RF setup, then send a valid packet. | CRC/error is counted or surfaced; Pocket Deck restarts listening and receives the following valid packet, or exposes a persistent error code if recovery fails. | Pending | No payload text. |
| LORA-11 | TF coexistence | With a TF card inserted and diagnostics active, exchange packets in both directions. | TF diagnostics continue; radio remains functional; no payload is present in diagnostics. | Pending | |
| LORA-12 | TF restart | Restart with TF inserted, then repeat a two-endpoint receive/send check. | Both TF logging and radio recover; chip selects coexist on shared SPI. | Pending | Do not erase or format the card. |
| LORA-13 | BLE regression | After LORA has initialized, connect/use Keyboard. | BLE remains responsive and no local LORA keystrokes leak as HID. | Pending | |
| LORA-14 | Wi-Fi/weather regression | After LORA has initialized, scan/connect or use existing Wi-Fi/weather paths. | Wi-Fi, clock, and weather remain responsive. | Pending | Do not expose SSIDs or coordinates. |
| LORA-15 | SSH regression | After LORA has initialized, open and use the SSH Terminal. | SSH remains reachable and responsive; no terminal content is logged. | Pending | |

## Result summary

| Area | Status | Blocking issue / follow-up |
|---|---|---|
| Automated preflight | Pass | Native/build/whitespace only; no RF claim |
| Cap and antenna switch | Pending | |
| Two-endpoint P2P interoperability | Pending | |
| Error recovery and receive re-arm | Pending | |
| TF shared-SPI coexistence | Pending | |
| BLE, Wi-Fi/weather, and SSH regression | Pending | |
