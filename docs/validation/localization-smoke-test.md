# Runtime localization smoke test

Target firmware: `0.9.6` on M5Stack Cardputer Adv.

- [ ] Existing v0.8.0 settings, Wi-Fi profiles, SSH hosts, and BLE bond survive upload.
- [ ] Open Settings > System; the first row shows `Language EN`.
- [ ] Press Enter; Settings immediately changes to Simplified Chinese.
- [ ] Return Home; every Launcher card title, subtitle, and hint renders correctly.
- [ ] Reopen Settings and visit Wi-Fi, Bluetooth, System, Storage, Diagnostics,
      password entry, saved networks, and every confirmation page.
- [ ] No Chinese text is clipped, replaced by boxes, or drawn over adjacent fields.
- [ ] SSIDs, IP addresses, diagnostics, version, time, and battery remain readable.
- [ ] Restart; Simplified Chinese remains selected.
- [ ] Open GPS and visit all four pages; titles, state labels, field labels,
      page hint, fix mode/quality, and all eight compass directions are Chinese.
- [ ] GPS coordinates, UTC values, units, HDOP, UART pins, and NMEA counters
      remain readable, with no clipping, overlap, boxes, or missing glyphs.
- [ ] Switch back to English and restart; English remains selected.
- [ ] Reopen all four GPS pages; all localized labels return to English.
- [ ] Open Weather with and without Wi-Fi, a fresh GPS fix, and cached data;
      state titles, details, conditions, fields, cache age, and hints are Chinese.
- [ ] Weather temperatures, percentages, `km/h`, observation time, and
      Open-Meteo attribution remain readable without clipping or overlap.
- [ ] Switch to English and confirm the complete Weather page returns to English.
- [ ] Open Keyboard while Bluetooth is off, advertising, pairing, connected,
      and in an error state; state, explanation, and footer follow the language.
      Pairing digits and `CTRL`/`SHIFT`/`OPT`/`CMD` remain literal.
- [ ] Open SSH with no key, no hosts, and a saved host; host-list prompts,
      editor field labels, delete confirmation, connection stages, known errors,
      retry countdown, and history marker follow the language.
- [ ] SSH host values, commands, terminal input/output, ANSI rendering, and
      `NVS!` remain literal and unchanged.
- [ ] Open LoRa with the Cap unavailable and available; app title, radio state,
      busy-send rejection, and footer follow the language. Frequency, SF,
      RSSI/SNR, draft, and RX/TX payload history remain literal.
- [ ] Open MEDIA with no card, empty folders, valid tracks, playback, pause, and
      a known error; state, detail, volume, and footer follow the language.
- [ ] MEDIA filenames, paths, elapsed time, progress, and list markers remain
      readable and unchanged in either language.
- [ ] Long-press G0; Quick Settings title, labels, values, and footer follow the
      language without clipping.
- [ ] Return Home from every app; the selected language remains active.
- [ ] Play MEDIA audio and confirm localization adds no playback stutter.

Compilation alone does not pass any row above.
