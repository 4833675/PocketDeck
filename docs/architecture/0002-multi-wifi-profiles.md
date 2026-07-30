# ADR-0002: Multi-profile Wi-Fi manager

**Status:** Accepted

**Date:** 2026-07-27

**Decider:** KEXIN

## Context

The original Wi-Fi service delegated persistence and reconnection to the ESP32
station stack. That retained only one SSID/password. When the saved access point
was unavailable, its continuing association attempt could also make a manual
scan fail with `WIFI_SCAN_FAILED`, preventing selection of a network at a new
location.

## Decision

- Store at most eight SSID/password profiles in a versioned, checksummed NVS
  blob owned by Pocket Deck.
- Keep Arduino Wi-Fi persistence and automatic reconnect disabled. `WifiService`
  is the sole owner of scan, candidate, connection, fallback, and retry state.
- Save or update a profile only after `WL_CONNECTED`; failed new credentials
  never replace a known-good profile.
- Before scanning while disconnected, cancel any pending association and retry
  scan startup for a bounded four-second window while the driver settles.
- At boot and after link loss, scan first, rank visible saved SSIDs by RSSI, and
  try each candidate in order. If none are available, scan again after 15 seconds.
- Keep scans asynchronous and use fixed-capacity arrays so BLE HID and rendering
  remain responsive and heap behavior stays predictable.
- Expose only SSIDs in UI snapshots. Passwords remain inside the Wi-Fi service
  and are never included in display, serial, BLE, weather, or diagnostic data.
- Import the legacy ESP32 station credential once when no Pocket Deck profiles
  exist, then erase the legacy slot after the new record is safely written.

## Consequences

- Pocket Deck can move among eight known 2.4 GHz networks without re-entering
  passwords and can fall back when the strongest candidate rejects a connection.
- A radio scan briefly consumes shared 2.4 GHz airtime; periodic disconnected
  scans are therefore limited to one every 15 seconds.
- Adding a ninth successful network evicts the least recently successful profile.
- Hidden SSIDs with no name in scan results are not automatic candidates in this
  version.
- The NVS partition is not encrypted by this firmware. Physical possession plus
  flash extraction may expose stored Wi-Fi credentials, as with the former ESP32
  station record.
