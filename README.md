# Pocket Deck

Pocket Deck is standalone, keyboard-first firmware for the **M5Stack Cardputer
Adv** (Stamp-S3A, 240×135 display, 8 MB flash). It turns the device into a small
system deck with a secure macOS Bluetooth keyboard, Pocket SSH terminal, GPS,
LoRa, TF-card MP3 playback, multi-profile Wi-Fi, time, weather, settings, and
diagnostics.

It is a from-scratch project, not a Claude Desktop Buddy fork, and it does not
support the original Cardputer model.

Current firmware: **0.8.0**

## Features

- Graphite Mint launcher, status bar, Quick Settings, and local system controls.
- Secure BLE HID keyboard with one bonded Mac and a stable `Pocket Deck` identity.
- Up to eight saved 2.4 GHz Wi-Fi networks with strongest-known selection and
  fallback.
- NTP clock and GPS-local weather from Open-Meteo.
- Interactive SSH terminal with six saved hosts, public-key authentication,
  ANSI colors, local scrollback, reconnect, and quick commands.
- Four-page GNSS dashboard for the optional M5Stack Cap LoRa-1262.
- Raw LoRa P2P text terminal for a matching SX1262/RadioLib peer.
- Foreground MP3 player with four-level folder browsing and Chinese filenames
  under `/Music` on a TF card.
- Foreground resource profiles: each app runs only the radios and peripherals
  it needs; MEDIA suspends wireless/GPS/LoRa work and defers TF event writes.
- Versioned Preferences/NVS settings and confirmed destructive actions.
- On-device diagnostics plus privacy-safe event logs for every app/service on a
  TF/microSD card.

Not currently implemented: microphone/dictation, LoRaWAN, IR, SFTP, SSH tunnels,
MQTT, Home Assistant, OTA updates, or Claude integration.

## Hardware

Required:

- M5Stack Cardputer Adv / Stamp-S3A
- USB-C data cable

Optional:

- M5Stack Cap LoRa-1262 for GPS, location-based weather, and LoRa P2P
- FAT-formatted TF/microSD card for MP3 playback and persistent diagnostics

The target has no PSRAM. The firmware uses one 3 MB application partition and
has no OTA slot. No LittleFS assets are currently required.

## Build and flash

Install PlatformIO, clone the repository, and connect the Cardputer Adv:

```bash
git clone https://github.com/4833675/PocketDeck.git
cd PocketDeck
scripts/test-native.sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
```

SSH is optional. To include SSH support in your local firmware image, configure
a private key as described in [Add an SSH private key](#add-an-ssh-private-key)
before building. A build without a readable key still succeeds, but SSH
connections are disabled.

Open the serial console at 115200 baud:

```bash
pio device monitor -e cardputer-adv
```

`uploadfs` is unnecessary. For ordinary updates, power the device off, connect
USB, and upload. If no serial port appears or upload remains at `Connecting...`,
enter recovery download mode: power off, hold G0, connect USB, then release G0.

## Controls

The Cardputer has no dedicated arrow cluster, so local navigation uses Fn:

| Control | System action |
|---|---|
| Fn + `;` | Up |
| Fn + `,` | Left |
| Fn + `.` | Down |
| Fn + `/` | Right |
| Enter | Open / confirm |
| Backspace | Back / cancel |
| G0 tap | Home |
| G0 hold for 600 ms | Quick Settings |

Home contains Keyboard, SSH Terminal, GPS, LORA, MEDIA, Weather, and Settings.
The status bar shows local 24-hour time, `WiFi`, `BT`, and battery percentage.
Wi-Fi and Bluetooth are mint when connected, amber when active but
disconnected, red when explicitly disabled, and muted gray when the foreground
app has suspended them.

Quick Settings uses Left/Right for brightness, Up/Down for volume, Enter to
toggle Bluetooth, and Backspace or G0 to close and save.

## Bluetooth keyboard

1. Open Keyboard from Home.
2. On the Mac, open System Settings > Bluetooth and select `Pocket Deck`.
3. Enter the six-digit code shown by Pocket Deck into the Mac dialog.
4. Wait for `CONNECTED` and the encrypted BLE HID status.

Pocket Deck intentionally stores one host bond. To change computers, use
Settings > Bluetooth > Forget host. Keyboard reports are sent only while the
Keyboard app is foreground and the link is authenticated; keys pressed on Home,
Settings, or while disconnected are never replayed to the Mac.

Modifier mapping:

| Cardputer | macOS |
|---|---|
| Ctrl | Control |
| Shift | Shift |
| Opt | Option |
| Alt | Command |
| Fn | Local layer selector |

Fn adds Escape, F1–F12, Forward Delete, Caps Lock, and arrow keys:

| Combination | HID key |
|---|---|
| Fn + `` ` `` | Escape |
| Fn + `1` … `0` | F1 … F10 |
| Fn + `-` / `=` | F11 / F12 |
| Fn + Backspace | Forward Delete |
| Fn + Tab | Caps Lock |
| Fn + `;` `,` `.` `/` | Up / Left / Down / Right |

All ordinary letters, numbers, punctuation, Space, Tab, Enter, and Backspace use
standard US keyboard HID usages.

## Pocket SSH terminal

SSH requires an active Wi-Fi connection and a private key embedded at build
time. Open SSH Terminal from Home, then press `N` to create a host with a label,
hostname/IP, username, and port. Up to six hosts are stored in NVS; `E` edits,
`D` deletes after confirmation, and Enter connects. Recently used hosts move to
the top.

### Add an SSH private key

Pocket Deck supports public-key authentication only. It does not read your
computer's `~/.ssh/config`, and it does not copy host aliases, usernames, or
ports from that file. Those fields are entered separately on the device.

The firmware cannot prompt for a key passphrase, so use an **unencrypted,
text-format private key**. A dedicated key is recommended because anyone who
obtains the flashed firmware may be able to extract it. To generate a dedicated
3072-bit RSA key without a passphrase:

```bash
ssh-keygen -t rsa -b 3072 \
  -f "$HOME/.ssh/pocketdeck_id_rsa" \
  -N "" -C "pocket-deck"
chmod 600 "$HOME/.ssh/pocketdeck_id_rsa"
```

If you already have an unencrypted private key, you can reuse it. If its `.pub`
file is missing, recreate the public key without exposing the private key:

```bash
ssh-keygen -y -f "$HOME/.ssh/id_rsa" > "$HOME/.ssh/id_rsa.pub"
```

Install **only the public key** on the remote account. Use `ssh-copy-id` when it
is available:

```bash
ssh-copy-id -i "$HOME/.ssh/pocketdeck_id_rsa.pub" USER@HOST
```

Alternatively, append it with ordinary SSH:

```bash
cat "$HOME/.ssh/pocketdeck_id_rsa.pub" | \
  ssh USER@HOST 'umask 077; mkdir -p ~/.ssh; cat >> ~/.ssh/authorized_keys'
```

Confirm that the same key works from the build computer before flashing:

```bash
ssh -i "$HOME/.ssh/pocketdeck_id_rsa" USER@HOST
```

Select that private key for the Pocket Deck build and flash the firmware:

```bash
export POCKETDECK_SSH_KEY="$HOME/.ssh/pocketdeck_id_rsa"
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
```

Without `POCKETDECK_SSH_KEY`, the build script automatically tries
`~/.ssh/id_rsa`. Look for `[ssh-key] private key available` in the PlatformIO
output. The script creates the generated byte-array header only under the
ignored `.pio/` directory; it never copies the private key into tracked source.

The generated firmware binary **does contain the complete private key**. Do not
commit or publish firmware binaries, and do not share a binary built with your
key. Factory reset clears saved SSH host records but cannot remove the compiled
key; replacing or removing it requires rebuilding and reflashing. If the device
or firmware is exposed, remove the corresponding public key from every server's
`authorized_keys` file.

Terminal controls:

| Control | SSH terminal action |
|---|---|
| Ordinary keys / Shift | Send text |
| Ctrl + A … Z | Send control byte, including Ctrl+C and Ctrl+D |
| Enter / Backspace / Tab | CR / DEL / Tab |
| Fn + `` ` `` | Escape |
| Fn + Backspace | Forward Delete |
| Fn + `;` `,` `.` `/` | Up / Left / Down / Right |
| Fn + Tab | Open quick commands (`uptime`, `df -h`, `free -h`, `docker ps`) |
| Opt + Fn + `;` / `.` | Scroll local history up / down |
| G0 | Home and disconnect |

The first version provides one interactive 40×13 PTY and 64 lines of local
scrollback. It implements a practical ANSI/VT100 subset; complex full-screen
programs and non-ASCII text may not render perfectly.

Per the current device-owner decision, SSH server host-key verification is
temporarily disabled. This permits man-in-the-middle attacks on an untrusted
network. The intended follow-up is TOFU fingerprint storage. The private key and
terminal input/output are never written to diagnostics or TF logs.

## Wi-Fi, clock, and weather

Open Settings > Wi-Fi > Scan networks. Results are strongest first; `S` marks a
saved network. Select a new secured network to enter its password locally, or
select an `S` row to reconnect without typing it again.

Pocket Deck stores up to eight successful profiles. Failed credentials are not
saved. At boot and after link loss it scans all visible SSIDs, tries saved
networks by signal strength, falls back after a timeout, and rescans every 15
seconds if none are available. Saved networks allows individual deletion;
Factory reset removes all profiles.

Only 2.4 GHz networks are supported. Passwords remain on the device and are
never included in BLE reports, weather requests, screen diagnostics, or logs.

After connection, NTP supplies the status-bar clock. Weather uses a fresh GPS
position to retrieve current conditions, feels-like temperature, humidity,
wind, daily high/low, and sunrise/sunset from
[Open-Meteo](https://open-meteo.com/en/docs). Successful weather data remains
visible from RAM when GPS or Wi-Fi later disappears; fresh inputs are required
only for the next update.

## GPS / GNSS

Attach the optional Cap LoRa-1262 and open GPS. Pocket Deck reads its NMEA stream
at 115200 baud on GPIO15 (RX) and GPIO13 (TX). Parsing continues in the
background.

Use Left/Right or Tab across four pages:

1. Position, altitude, satellites, HDOP, and fix age.
2. UTC date/time, speed, course, direction, mode, and quality.
3. UART state, received characters, valid checksums, checksum errors, and fix
   sentence counts.
4. Large speed in `KM/H` and course in degrees plus compass point. A stale or
   invalid motion field displays `--`.

`RX CHARS` rising proves bytes are arriving; `CHECKSUM OK` rising proves valid
NMEA. A valid stream can still show `SEARCHING` until the ceramic antenna has a
clear outdoor view and acquires satellites. Cold starts can take several
minutes.

## LoRa text terminal

With the optional Cap LoRa-1262 attached, open LORA from Home. **Attach the Cap
antenna before opening LORA, initializing the radio, or transmitting.** Enter
printable ASCII into the 120-byte draft, use Backspace to edit, Enter to send a
non-empty draft while `LISTENING`, and Fn+`` ` `` to return Home. A busy radio
rejects additional Enter presses rather than queueing duplicates.

This is raw LoRa P2P, not LoRaWAN. It transmits exact draft bytes without a
Pocket Deck header and uses no encryption, ACKs, addressing, retries, or delivery
guarantee. It is half-duplex. A second SX1262 endpoint running RadioLib with the
same profile is required for interoperability: **868.0 MHz, 125.0 kHz, SF12,
coding rate 4/5, sync word `0x34`, +22 dBm, 20-symbol preamble, 3.0 V TCXO, and
140 mA current limit**. Check that this RF configuration is permitted where you
use it.

See the [LoRa text-terminal hardware checklist](docs/validation/lora-text-terminal-smoke-test.md)
for the required two-endpoint, antenna, RF recovery, TF coexistence, and
regression evidence. A successful build does not prove radio operation.

## MEDIA MP3 player

MEDIA plays MP3 files from the Cardputer Adv TF card through the built-in
speaker or 3.5 mm audio output. Create `/Music` in the card root, then organize
tracks in folders such as `/Music/artist/album/song.mp3`. MEDIA browses up to
four folder levels, accepts `.mp3` case-insensitively, lists folders before
tracks, and keeps at most 64 entries from the current folder in RAM. Chinese
folder and track names use M5GFX's built-in `efontCN_14`; no font filesystem
asset is required.

| Control | MEDIA action |
|---|---|
| Fn + Up / Down | Select previous / next library row |
| Enter | Open a folder, play a track, or pause/resume the loaded track |
| Backspace | Return to the parent folder; from `/Music`, return Home |
| Fn + Left / Right | Play previous / next track in the current folder |
| `-` / `=` | Volume −5 / +5 and save the new level |
| Tab | Rescan while stopped |
| G0 | Stop, release the decoder, and return Home from any folder |

The screen shows active-play elapsed time and byte-position progress; variable
bitrate files can therefore make the percentage advance unevenly. End of file
automatically advances to the next track. Playback is foreground-only: leaving
MEDIA closes the MP3 and stops audio. Folder changes also stop playback before
opening a new SD directory. The first version has no ID3 display, cover art,
seeking, playlists, shuffle, background playback, or other codecs. For this
device workload, 128 kbps / 44.1 kHz MP3 is the recommended source format.

MEDIA reuses the already-mounted TF card instead of remounting it. TF logs and
the SX1262 share the existing SPI bus and remain serialized by the main system
loop. MP3 decoding uses
[ESP8266Audio 2.2.0](https://github.com/earlephilhower/ESP8266Audio/tree/2.2.0),
which is GPL-3.0 software; comply with that license when distributing firmware
binaries.

## Diagnostics and TF card

Settings > System > Diagnostics shows reset reason, memory and recent events.
For persistent history, insert a TF card and restart. The active log is
`/PocketDeck/system.log`. At 4 MB it rotates through `system-1.log`,
`system-2.log`, and `system-3.log`, retaining up to about 16 MB. Old
`ble.log`/`ble-prev.log` files from earlier firmware remain readable with
`LOG DUMP ALL`.

Logging is event-based rather than frame- or keystroke-based. It records boot,
application switches, MEDIA scans/folder depth/playback state, BLE, Wi-Fi, GPS
receiver health, weather, SSH, LoRa, Settings, and TF storage events without
adding continuous SD writes to the main loop.

Settings > System > TF card logs can retry mounting or format the card after a
separate confirmation. Formatting erases the entire card.

Logs can be read over USB without removing the card:

| Serial command | Action |
|---|---|
| `HELP` | List commands |
| `LOG STATUS` | Show card/logger state |
| `LOG DUMP` | Print the current log |
| `LOG DUMP ALL` | Print archived, legacy, and current logs |
| `LOG CLEAR YES` | Erase all logs and start a new session |

Typed keyboard/SSH content, Wi-Fi passwords, pairing codes, precise GPS
coordinates, MEDIA filenames/audio, and LoRa payloads are deliberately excluded
from diagnostics.

## Reset and stored data

- Restart preserves settings, Wi-Fi profiles, and the BLE bond.
- Disabling Wi-Fi preserves profiles; Saved networks deletes one selected SSID.
- Forget host removes the single BLE bond and starts a new pairing flow.
- Factory reset clears application settings, all Wi-Fi profiles, all SSH host
  entries, and the BLE bond, then restarts. It does not format the TF card and
  cannot remove the private key compiled into firmware.

## Troubleshooting

- **No Bluetooth device:** open Keyboard and confirm Bluetooth is enabled and the
  screen says `PAIRING` or `ADVERTISING`.
- **Connected but not typing:** Keyboard must be foreground and show `CONNECTED`.
- **Another Mac cannot pair:** remove the existing bond with Forget host first.
- **Wi-Fi list is empty:** ensure Wi-Fi is on, open Scan networks, press Tab, and
  verify the access point offers 2.4 GHz.
- **GPS says `NO DATA`:** reseat the Cap and check page 3/4; `RX CHARS` must rise.
- **GPS remains `SEARCHING`:** test outdoors with the antenna facing open sky.
- **LORA says `RADIO NOT FOUND`:** confirm the Cap is seated and its antenna is
  attached before retrying. Do not transmit without an antenna.
- **MEDIA says `NO TF CARD`:** mount the card under Settings > System > TF card
  logs, then return to MEDIA and press Tab.
- **MEDIA says `NO MP3 FILES`:** copy MP3 files into `/Music` or a folder no
  deeper than four levels; other audio formats are ignored. Firmware before
  0.7.0 cannot see nested folders.
- **MEDIA says `OUT OF MEMORY`:** restart Pocket Deck before playing, especially
  after opening SSH; the device has no PSRAM.
- **Weather will not update:** verify a fresh GPS fix, Wi-Fi/IP, and NTP, then
  press Enter in Weather.
- **SSH key missing:** rebuild with a readable `POCKETDECK_SSH_KEY` path or
  `~/.ssh/id_rsa`.
- **SSH authentication fails:** add the matching public key to the remote
  account's `authorized_keys`, then verify the on-device username and port.
- **Early serial output is missing:** native USB re-enumerates after reset; use
  on-device diagnostics or TF logs for boot history.
- **Incremental build acts stale:** run `pio run -e cardputer-adv -t clean`, then
  build again. A full flash erase is not a normal troubleshooting step.

## Development documentation

- [Current project handoff](MEMORY.md)
- [Agent/developer workflow](AGENTS.md)
- [Architecture decisions](docs/architecture/)
- [Hardware validation checklists](docs/validation/)
- [GPS hardware checklist](docs/validation/gps-smoke-test.md)
- [LoRa text-terminal checklist](docs/validation/lora-text-terminal-smoke-test.md)
- [MEDIA MP3 checklist](docs/validation/media-mp3-smoke-test.md)
- [System event-log checklist](docs/validation/system-event-log-smoke-test.md)

Source layout:

```text
src/core/       lifecycle, state, policies, input routing, portable models
src/drivers/    Cardputer board and display adapters
src/services/   BLE, Wi-Fi, SSH, GPS, LoRa, MEDIA, weather, NVS, TF diagnostics
src/apps/       launcher, Keyboard, SSH, GPS, LORA, MEDIA, Weather, Settings
src/ui/         shared status bar and Quick Settings
test/native/    hardware-independent C++ tests
```
