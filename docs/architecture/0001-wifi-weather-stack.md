# ADR-0001: Wi-Fi and GPS-local weather stack

**Status:** Accepted  
**Date:** 2026-07-24  
**Decider:** KEXIN

## Context

Pocket Deck needs on-device Wi-Fi selection, saved station credentials, NTP
time, network diagnostics, and weather based on the existing Cap LoRa-1262 GPS.
The Cardputer Adv has no PSRAM, and BLE keyboard latency must remain acceptable
while Wi-Fi scans or performs HTTPS requests.

## Decision

- `WifiService` owns the ESP32 station radio, asynchronous scans, connection
  state, the one saved station profile, NTP setup, and a fixed-size snapshot.
- Settings renders snapshots and sends operations to `System`; password entry
  uses a separate local text input mode and never produces HID reports.
- The ESP32 Wi-Fi stack persists credentials in NVS. Application settings store
  only the Wi-Fi enabled flag.
- `WeatherService` requests Open-Meteo in a short-lived FreeRTOS task and
  publishes a fixed-size snapshot under a critical section.
- Weather refreshes from a fresh GPS position and never substitutes an inferred
  or hard-coded location.
- The first weather transport uses TLS without certificate pinning. It carries
  no credential or typed content; this avoids certificate-rotation outages but
  means forecast authenticity is not guaranteed against an active network
  attacker.

## Options considered

### Blocking Wi-Fi and HTTP in the main loop

Simpler, but a slow scan, DHCP exchange, TLS handshake, or HTTP response would
stall rendering and local input. Rejected.

### Captive portal only

Convenient from a phone, but it adds a second AP/server workflow and does not
exercise the Cardputer keyboard-first interface. Deferred as a later fallback.

### API-key weather provider

Can offer richer data, but requires secret provisioning and rotation. Rejected
for the first implementation in favor of Open-Meteo's keyless forecast API.

## Consequences

- BLE and screen updates continue while weather HTTPS is blocked in its worker.
- Wi-Fi passwords remain out of application logs and BLE reports.
- Wi-Fi and BLE still share the ESP32-S3 2.4 GHz radio, so scans are explicit
  rather than continuous.
- Future certificate verification, captive-portal provisioning, SSH, MQTT, and
  Home Assistant can extend the service layer without changing app ownership.
