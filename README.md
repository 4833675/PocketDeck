# Pocket Deck

Pocket Deck is a standalone, keyboard-first system for the M5Stack Cardputer Adv.
It is a new project and does not depend on Claude Desktop or Claude Buddy firmware.

## Current implementation phase

Foundation and secure single-host BLE keyboard for macOS.

## Build

```bash
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -e cardputer-adv
```

The firmware targets Cardputer Adv only and does not use PSRAM.
