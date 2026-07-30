# Pocket Deck System Event Log Hardware Smoke Test

Target firmware: `0.7.1` on M5Stack Cardputer Adv with a disposable or backed-up
FAT32 TF card. Do not format or clear logs while diagnosing an unresolved issue.

| ID | Action | Expected evidence | Status | Notes |
|---|---|---|---|---|
| LOG-01 | Restart with TF inserted | `system.log` starts with the firmware version, reset reason, board/services, and `App active: LAUNCHER` | Pending | |
| LOG-02 | Open every launcher app and Quick Settings | Each app switch and Quick Settings open/close is recorded | Pending | |
| LOG-03 | Connect/disconnect BLE and Wi-Fi; run a scan | State transitions, authentication/disconnect reason, and aggregate scan counts appear | Pending | |
| LOG-04 | Leave GPS receiving for over 60 seconds | GPS state plus `rx/ok/err/fix/sats` health counters appear without coordinates | Pending | |
| LOG-05 | Refresh Weather and open/close SSH | Weather and SSH lifecycle/errors appear without location, host, user, or terminal data | Pending | |
| LOG-06 | Open LoRa and send/receive with a valid peer | Radio operation, status, lengths, and counters appear without payload text | Pending | |
| LOG-07 | Browse MEDIA folders, play, pause, skip, and exit | Scan depth/entry counts, playback state/index, and release are recorded without filenames | Pending | |
| LOG-08 | Change volume and a safe Settings toggle | The setting change and resulting service transition appear | Pending | |
| LOG-09 | Send `LOG STATUS`, `LOG DUMP`, and `LOG DUMP ALL` over USB | Current and legacy/archived logs can be read; logger stays ready | Pending | |
| PRIV-LOG-01 | Inspect serial and all TF logs | No passwords, pairing code, key/HID/SSH text, exact coordinates, filenames/audio, LoRa payload, or private key | Pending | |

Rotation is covered by code/build validation unless a disposable card is used to
create a file over 4 MB; do not inflate the owner's normal log merely to test it.
