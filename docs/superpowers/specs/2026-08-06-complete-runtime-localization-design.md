# Complete Runtime Localization Design

## Goal

Finish the English / Simplified Chinese runtime UI for every existing Pocket
Deck feature that can be translated without changing a protocol, payload, user
value, shell command, or diagnostic record. Future user-visible features must
ship in both languages in the same change unless a documented technical limit
makes that impossible.

## Established decision

Pocket Deck remains one `main` branch and one firmware image. The existing
`UiLanguage` setting selects English or Simplified Chinese immediately and is
persisted in NVS. English uses `Font0`; Chinese uses the already-linked
`efontCN_14`. There is no dynamic catalog, filesystem language pack, second
firmware edition, or permanent language branch.

## Translation boundary

Translate product-owned, user-facing text:

- application and overlay titles;
- state, empty, confirmation, progress, retry, and error labels;
- field names and operation hints;
- known service errors that are rendered directly on screen.

Keep compatibility-sensitive or user-owned text unchanged:

- SSH terminal output and quick-command command lines;
- host labels, usernames, hostnames, ports, SSIDs, IP addresses, paths, and
  filenames;
- LoRa payloads, `RX`/`TX`, frequency, spreading factor, `RSSI`, `SNR`, and
  radio status codes;
- GPS coordinates, `NMEA`, `HDOP`, units, UART pins, and raw counters;
- log and diagnostics event text, reset codes when shown in raw diagnostics,
  `NVS`, `NTP`, `TF`, `BT`, and `WiFi` identifiers.

## Components

Each app owns a small pure text adapter beside its renderer:

- `keyboard_app_text.*`: BLE keyboard states and errors;
- `ssh_app_text.*`: SSH states and error categories;
- `lora_app_text.*`: radio states;
- `media_app_text.*`: playback states and known service details;
- `settings_app_text.*`: reset-reason and TF-storage error presentation.

Adapters accept `UiLanguage` and enum or stable service data. English mappings
remain behavior-compatible with the current UI. Chinese mappings use fixed
string literals and allocate no heap. Unknown service details pass through
unchanged so a new error never disappears.

Quick Settings contains only fixed labels and uses `localized()` directly. Its
`render()` call receives the current language from `System`, matching every app
renderer. Launcher reuses the GPS and Weather adapters instead of maintaining
duplicate translations.

## Layout and fonts

Every localized renderer explicitly selects `setUiFont()` before user-facing
text and `setTechnicalFont()` before fixed-width technical content. Chinese
centered state titles use one-times `efontCN_14` instead of scaling the glyphs.
Terminal cells, LoRa payload rows, media paths, passkeys, command strings, and
numeric meters keep their technical font and geometry.

Hints are shortened for 240×135 rather than translated word-for-word. Existing
input behavior and key bindings do not change.

## Error handling

Known BLE, SSH, MEDIA, and TF errors receive Chinese labels. Unknown details
remain visible verbatim. Localization never changes service state, retry policy,
storage contents, pairing, network behavior, or diagnostic logs.

## Validation

Native tests exercise every enum branch and every known detail mapping in both
languages. The red phase must fail because the new adapters are absent, then the
same tests must pass after implementation. Final validation requires:

1. `scripts/test-native.sh`
2. `git diff --check`
3. `pio run -e cardputer-adv`
4. ordinary upload when `/dev/cu.usbmodem*` is available

Hardware validation remains a separate checklist: inspect both languages for
glyph coverage, clipping, mixed-font alignment, and unchanged controls.

## Durable rule

The bilingual-by-default requirement is recorded in repository `AGENTS.md`,
the localization ADR, project `MEMORY.md`, and a Codex long-term-memory update.
An exception must be technically necessary, explained to the user, and leave
technical or user-owned content readable rather than silently omitting it.
