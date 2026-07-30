# Pocket Deck MEDIA MP3 Hardware Smoke Test

Target firmware: `0.6.0` on M5Stack Cardputer Adv.

Use short disposable MP3 files under `/Music`. Do not record filenames in this
checklist or diagnostics. Never format the TF card as part of this checklist.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| MEDIA-01 | Open MEDIA with no mounted TF card | `NO TF CARD`; UI and Home remain responsive | Pending | |
| MEDIA-02 | Mount an empty FAT card and open MEDIA | `/Music` is created and `NO MP3 FILES` appears | Pending | |
| MEDIA-03 | Add mixed-case `.mp3` names plus non-MP3 files | MP3 files only; deterministic case-insensitive order | Pending | |
| MEDIA-04 | Add more than 32 MP3 files | 32 entries shown with a `+` capacity marker; no crash | Pending | |
| MEDIA-05 | Play a valid MP3 through the built-in speaker | Clear continuous playback; progress and elapsed time rise | Pending | |
| MEDIA-06 | Pause and resume with Enter | Audio stops promptly and resumes from the same decoder position | Pending | |
| MEDIA-07 | Use previous/next and allow one file to finish | Manual skip and automatic next each start exactly one track | Pending | |
| MEDIA-08 | Adjust volume with `-` / `=`, restart | Audible level changes and saved percentage survives restart | Pending | |
| MEDIA-09 | Insert 3.5 mm headphones or an external speaker | Output switches away from the internal speaker | Pending | |
| MEDIA-10 | Try a corrupt or unsupported `.mp3` file | Bounded error; another track can still be selected | Pending | |
| MEDIA-11 | Leave MEDIA during playback | Audio stops, file closes, Home and other apps remain responsive | Pending | |
| MEDIA-12 | Keep TF diagnostics active during playback | Log appends/rotation do not corrupt playback or the filesystem | Pending | |
| MEDIA-13 | With antenna attached, exercise LORA after MEDIA | SX1262 still initializes and shared SPI remains usable | Pending | |
| MEDIA-14 | Keep BLE and Wi-Fi connected during playback | Keyboard reconnect, status bar, and UI remain responsive | Pending | |
| MEDIA-15 | Open/close an SSH shell, then start MEDIA | Plays or shows `OUT OF MEMORY` cleanly; no reset or hang | Pending | |
| PRIV-MEDIA-01 | Inspect serial and TF diagnostics | No track filename or audio content is logged | Pending | |
