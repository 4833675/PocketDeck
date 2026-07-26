# Pocket Deck

Pocket Deck is standalone, keyboard-first firmware built specifically for the
**M5Stack Cardputer Adv** (Stamp-S3A, 240x135 display, 8 MB flash). It is not
intended for the original Cardputer model.

The Bluetooth keyboard and system shell run on the Cardputer Adv alone. The GPS
dashboard and GPS-local weather additionally require the optional **M5Stack Cap
LoRa-1262**, which contains the supported GNSS receiver.

The current `0.3.5` firmware contains a Graphite Mint system shell, launcher,
secure single-host Bluetooth LE keyboard for macOS, a live GNSS/GPS app for the
Cap LoRa-1262, Wi-Fi scanning and connection, NTP time, GPS-local weather,
Bluetooth/Wi-Fi/System settings, Quick Settings, persistent preferences,
on-device diagnostics, and optional persistent BLE diagnostics on microSD.

Hardware validation procedures are kept under [`docs/validation/`](docs/validation/).

## Target and boundaries

- M5Stack Cardputer Adv / Stamp-S3A, 8 MB flash, 240×135 display.
- Optional M5Stack Cap LoRa-1262 for GPS and location-based weather.
- PlatformIO + Arduino-ESP32, M5Cardputer 1.1.1, and M5Unified 0.2.17.
- One 3 MB application partition plus LittleFS; no OTA slot and no PSRAM.
- One stable BLE HID identity named `Pocket Deck` and one bonded Mac.
- English UI with built-in fonts; no filesystem assets are currently required.
- No microphone, dictation, Claude integration, LoRa radio, IR, SSH terminal,
  MQTT, or Home Assistant integration yet.

## Prerequisites

Install PlatformIO, clone the repository, and connect the Cardputer Adv over
USB:

```bash
git clone https://github.com/4833675/PocketDeck.git
cd PocketDeck
```

## Test, build, flash, and monitor

Run the hardware-independent keymap, routing, settings, navigation, and policy
tests:

```bash
scripts/test-native.sh
```

Build the firmware:

```bash
pio run -e cardputer-adv
```

Flash the connected Cardputer Adv:

```bash
pio run -e cardputer-adv -t upload
```

Open the 115200-baud serial monitor:

```bash
pio device monitor -e cardputer-adv
```

Use `Ctrl+C` to leave the monitor. Native USB disconnects briefly during reset,
so a monitor attached after flashing can miss the earliest serial lines. The
same bounded diagnostic events are available under Settings > System >
Diagnostics. `uploadfs` is not needed in this phase because the UI has no
LittleFS assets.

## System controls

The Cardputer has no dedicated arrow cluster, so system navigation uses the Fn
layer:

| Control | System action |
|---|---|
| Fn + `;` | Up |
| Fn + `,` | Left |
| Fn + `.` | Down |
| Fn + `/` | Right |
| Enter | Open / confirm |
| Backspace | Back / cancel |
| G0 short press | Return directly to Home |
| G0 hold for 600 ms | Open Quick Settings |

Home is a horizontal card launcher. Use Fn+`,` and Fn+`/` to select Keyboard,
GPS, Weather, or Settings, then Enter to open it. Every boot returns to Home with
Keyboard selected; booting never makes ordinary keys active as HID input by
itself.

The status bar shows a 24-hour clock in the center and `WiFi`, `BT`, and battery
percentage on the right. Each radio label is mint while connected, amber while
enabled but disconnected, and red while deliberately off. The clock shows
`--:--` until NTP has supplied a plausible time.

Quick Settings is an overlay over the current app:

| Control | Quick Settings action |
|---|---|
| Left / Right | Brightness −10 / +10 |
| Up / Down | Volume +10 / −10 |
| Enter | Toggle BLE keyboard service |
| Backspace or G0 short | Close and persist changed values |

Opening Quick Settings from Keyboard first sends an all-keys-up report. When it
closes, Keyboard waits until locally held keys are released before forwarding
input again.

## Pair one Mac

1. Boot Pocket Deck and press Enter on the selected Keyboard card.
2. Confirm that the Keyboard screen says `PAIRING` and shows a six-digit code.
3. On the Mac, open System Settings > Bluetooth and select `Pocket Deck`.
4. When macOS asks for the pairing code, enter the six digits shown on Pocket
   Deck into the Mac dialog. Do not type the code on the Cardputer.
5. Wait for Pocket Deck to show `CONNECTED` and `encrypted BLE HID`.

The bond is stored by the ESP32 BLE stack and should reconnect after a Pocket
Deck restart without changing the device identity. Pocket Deck intentionally
accepts only one bonded host. It stores the local label `Mac`; it does not claim
to discover or persist the computer's human-readable Bluetooth name.

Keys are sent only while Keyboard is the foreground app and the BLE link is
encrypted. Input is discarded while disconnected and is not replayed later.
The screen and diagnostics never display or log typed characters, matrix state,
or HID reports.

## macOS keyboard mapping

| Physical key | macOS meaning |
|---|---|
| Ctrl | Left Control |
| Shift | Left Shift |
| Opt | Left Option |
| Alt | Left Command |
| Fn | Local layer selector; never sent to the Mac |

The ordinary letter, number, punctuation, Space, Tab, Enter, and Backspace keys
use standard US keyboard HID usages. The local Fn layer adds:

| Combination | HID key |
|---|---|
| Fn + `` ` `` | Escape |
| Fn + `1` … `0` | F1 … F10 |
| Fn + `-` | F11 |
| Fn + `=` | F12 |
| Fn + Backspace | Forward Delete |
| Fn + Tab | Caps Lock |
| Fn + `;` | Up Arrow |
| Fn + `,` | Left Arrow |
| Fn + `.` | Down Arrow |
| Fn + `/` | Right Arrow |

Without Fn, `` ` ``, `;`, `,`, `.`, and `/` remain ordinary punctuation.
Reports support the standard six simultaneous non-modifier keys plus the
modifier byte; larger chords produce the standard HID rollover report.

## TF card diagnostic logging

Pocket Deck can persist system and detailed BLE lifecycle events to the built-in
microSD slot. Insert a FAT-formatted card with its contacts facing away from the
screen, then boot the device. Logging starts automatically when the card mounts.
The SD bus is initialized conservatively and keeps the optional Cap LoRa-1262
chip-select inactive because that accessory shares the same SPI data pins.

Open Settings > System > TF card logs to see card status. `Mount / retry` handles
a card inserted after boot. `Format TF card` requires a separate confirmation
screen and erases all data on the card; it can also prepare a card that does not
yet contain a filesystem supported by the firmware.

The active file is `/PocketDeck/ble.log`. At 512 KB it rotates to
`/PocketDeck/ble-prev.log`, keeping storage bounded. Every event is flushed and
closed immediately so an unexpected shutdown does not lose a buffered session.
Lines contain UTC after NTP synchronization and always contain device uptime.
Power Pocket Deck off before removing the card to read it on another computer.

The same files can be read without removing the card. Connect USB, open the
115200-baud serial monitor, type a command, and press Enter:

| Serial command | Action |
|---|---|
| `HELP` | List the available serial-console commands |
| `LOG STATUS` | Show card, capacity, logger, and write status |
| `LOG DUMP` | Print the current `/PocketDeck/ble.log` |
| `LOG DUMP ALL` | Print the rotated log followed by the current log |
| `LOG CLEAR YES` | Erase both logs and immediately begin a new session |

`LOG CLEAR` without the final `YES` is deliberately rejected.

## GPS / GNSS

The GPS app supports the M5Stack Cap LoRa-1262 attached to the Cardputer Adv
14-pin expansion connector. Its ATGM336H/AT6668 receiver sends NMEA 0183 4.1 at
115200 baud. Pocket Deck reads Cap `GPS_TX` on GPIO15 and assigns GPIO13 as the
return UART pin. The GPS service runs continuously in the background, including
while Home, Keyboard, or Settings is visible.

Select GPS in Home and press Enter. Use Fn+`,` / Fn+`/` or Tab to move through
three pages:

1. Position: fix state, latitude, longitude, altitude, satellites, HDOP, fix
   age, and incoming-data age.
2. Motion/time: UTC date and time, speed in km/h, course in degrees and an
   eight-point compass direction, plus fix mode and quality.
3. Receiver: UART configuration, NMEA character count, valid sentence count,
   checksum error count, and sentences containing a fix.

The status distinguishes `NO DATA`, `NO STREAM`, `SEARCHING`, `STALE FIX`, and
`FIX`. On the receiver page:

- `RX CHARS` rising means bytes are arriving from the Cap.
- `CHECKSUM OK` rising means the baud rate and NMEA stream are valid.
- `CHECKSUM ERR` rising rapidly while OK remains zero suggests corrupted input
  or an incorrect module configuration.

GPS time is displayed as UTC rather than silently applying a local timezone.
The Cap uses a built-in ceramic GNSS antenna; first position acquisition should
be tested outdoors with a broad view of the sky. A cold start can take minutes
in real conditions even though valid NMEA data appears immediately.

## Wi-Fi, NTP, and network diagnostics

Open Settings > Wi-Fi. The Wi-Fi page provides an on/off switch, asynchronous
network scan, network diagnostics, and a confirmed forget action. Select a scan
result and enter its password directly on the Cardputer. Password input remains
local: it is masked on screen, never routed through BLE HID, and never written
to serial or the diagnostic ring. Use Backspace to erase a character and
Fn+Backtick to cancel entry.

The ESP32 Wi-Fi stack stores one selected station network in NVS. When Wi-Fi is
enabled after restart, Pocket Deck reconnects using that saved configuration.
Disabling Wi-Fi preserves it; Forget network and Factory reset erase it.

After connection Pocket Deck synchronizes UTC through NTP. Settings > Wi-Fi >
Network info displays connection state, SDK status, SSID, RSSI, local IP,
gateway, DNS, and NTP UTC time. The status-bar clock displays UTC+8 and keeps
advancing for the rest of the current boot if Wi-Fi later disconnects. Scanning
and connecting are non-blocking so the BLE keyboard and UI continue running
while Wi-Fi changes state.

## Weather

Weather uses the most recent valid GPS coordinates and the connected Wi-Fi
network. It retrieves current temperature, apparent temperature, humidity,
wind, WMO weather condition, today's high/low, and local sunrise/sunset from
[Open-Meteo](https://open-meteo.com/en/docs). No API key is required. Press
Enter to refresh manually; successful data refreshes automatically after 15
minutes or after moving roughly 0.05 degrees.

The last successful forecast remains visible in RAM if GPS or Wi-Fi later
disconnects. Its footer changes from `LIVE` to an amber cached status with its
age and the missing input. A failed refresh also preserves the previous data.
Fresh GPS and Wi-Fi are required to update the forecast, not to view it. The
cache is cleared on restart.

The HTTPS request runs in a separate FreeRTOS task so a slow forecast endpoint
does not block keyboard input or rendering. This first implementation accepts
the endpoint certificate without pinning because it transmits only public
coordinates and receives public forecast data; it sends no Wi-Fi password,
typed content, token, or other secret.

## Bluetooth, Wi-Fi, and System settings

Open Settings from Home, select a category with Up/Down, and press Enter.

Bluetooth provides:

- BLE enabled state, connection state, device name, local host label,
  encryption state, and bond state.
- `Disconnect`, which drops the current link but preserves the Mac bond.
- `Forget host`, which requires Enter on a confirmation page, disconnects,
  removes the one BLE bond, generates a new pairing code, and advertises again.

System provides:

- Firmware version, reset reason, uptime, free heap, minimum free heap, and the
  five newest entries from a fixed 12-entry diagnostic ring.
- `Restart`, protected by a confirmation page.
- `Factory reset`, protected by a separate confirmation page. It clears the
  `pocketdeck` application settings namespace, saved Wi-Fi network, and BLE
  bond, then restarts.

Settings are stored as a versioned, checksummed Preferences/NVS record. BLE
bond keys remain owned by the BLE stack and are never copied into application
settings. Missing or invalid settings safely restore defaults.

## Troubleshooting

- **No device in macOS Bluetooth:** open Keyboard and verify it says `PAIRING`
  or `ADVERTISING`, then verify Bluetooth is ON in Quick Settings or Settings.
- **Connected but no typing:** Keyboard must be the foreground app and its
  screen must say `CONNECTED`. Home and Settings always consume keys locally.
- **A different computer cannot pair:** Pocket Deck is intentionally single-host.
  Use Settings > Bluetooth > Forget host, confirm it, and pair the new Mac.
- **Reconnect behaves strangely:** use Disconnect first. Use Forget host only
  when deliberately replacing or repairing the bond.
- **Serial shows little after reset:** native USB may re-enumerate after early
  boot logs. Read Settings > System > Diagnostics or open the monitor before a
  manual reset.
- **GPS stays at `NO DATA`:** verify the Cap is fully seated and open GPS page
  3/3. `RX CHARS` must increase. This firmware expects the Cap LoRa-1262 default
  115200-baud configuration.
- **GPS says `SEARCHING`:** the serial/NMEA path is working; move outdoors with
  the ceramic antenna facing open sky and allow time for a cold start.
- **Wi-Fi scan is empty:** confirm Wi-Fi is ON, press Tab to scan again, and
  remember the ESP32-S3 supports 2.4 GHz networks rather than 5 GHz-only SSIDs.
- **Weather waits for GPS:** Weather deliberately requires a fresh GPS fix; open
  GPS outdoors before requesting a location-based forecast.
- **Weather reports an HTTP error:** inspect Settings > Wi-Fi > Network info,
  confirm NTP/IP are available, then press Enter in Weather to retry.
- **Need a clean compile:** run `pio run -e cardputer-adv -t clean`, then build
  again. A full flash erase is not part of the normal update path.

## Source layout

```text
src/core/       system loop, app lifecycle, input routing, settings model
src/drivers/    Cardputer board and display adapters
src/services/   BLE HID, Wi-Fi/NTP, weather HTTPS, GPS, Preferences, diagnostics
src/ui/         Graphite Mint widgets and Quick Settings
src/apps/       launcher, Keyboard, GPS, Weather, and Settings
test/native/    hardware-independent C++ tests
```

Architecture decisions and repeatable hardware validation checklists are kept
under `docs/`.
