# ADR 0006: Runtime localization from one firmware image

Status: accepted

## Context

The English and Simplified Chinese editions have identical hardware behavior
and features. Permanent language branches would require every radio, storage,
and UI fix to be copied between branches and would inevitably drift.

## Decision

Pocket Deck keeps one `main` branch and one firmware image. `UiLanguage` is a
fixed enum stored in the existing settings-record flag byte. Old records decode
as English, so adding language selection does not clear existing settings.

Localized text uses compile-time string literals and no heap-backed catalog.
English renders with `Font0`; Simplified Chinese reuses M5GFX `efontCN_14`,
which is already linked for MEDIA filenames. Pages explicitly choose their font
for every localized or technical region so font state cannot leak between apps.

All product-owned UI is localized: Launcher, Quick Settings, Keyboard, SSH,
all four GPS pages, LoRa, MEDIA, Weather, and the complete Settings workflow.
Technical and user-controlled content remains literal: terminal output and
commands, SSH host values, SSIDs and addresses, filenames and paths, LoRa
payloads and RF metrics, GPS coordinates/NMEA values, and raw diagnostic events.

New features must include English and Simplified Chinese in the same change by
default. An exception is allowed only when the platform cannot render or fit a
translation without breaking the feature; the limitation and reason must then
be documented explicitly.

## Consequences

- One bug fix and one release artifact serve both languages.
- Runtime RAM is unchanged; translation strings add only a small amount of Flash.
- Every newly localized page must be checked on the 240×135 display for width,
  line height, missing glyphs, and mixed technical text.
- Feature completion includes both languages; localization is not deferred as a
  separate follow-up task.
- Adding another full CJK font size or weight remains a separate Flash-budget
  decision; this ADR does not authorize it.
