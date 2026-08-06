# Pocket Deck BLE Keyboard Hardware Smoke Test

This is a reusable hardware-validation template for Pocket Deck `0.9.6` or
later. Automated tests and a successful firmware link do not mark a hardware
row as passed. Fill in every result after flashing a real M5Stack Cardputer Adv.

## Test record

| Field | Value |
|---|---|
| Date / time | Pending |
| Tester | Pending |
| Firmware commit | Pending |
| Firmware version | 0.9.6 or later |
| Cardputer model | M5Stack Cardputer Adv |
| Mac model | Pending |
| macOS version | Pending |
| Previous Pocket Deck bond removed before first-pair test | Pending |

Use only `PASS`, `FAIL`, or `BLOCKED` in the Result column. Put exact observed
behavior, screen text, and relevant diagnostic messages in Notes. Never record
the six-digit pairing code or typed test content in this file.

## Automated preflight

| Check | Command | Expected | Result | Notes |
|---|---|---|---|---|
| Native tests | `scripts/test-native.sh` | All checks pass | Pending | |
| Firmware build | `pio run -e cardputer-adv` | Exit 0; RAM/flash below limits | Pending | |
| Firmware upload | `pio run -e cardputer-adv -t upload` | Upload and reset succeed | Pending | |
| Serial / diagnostics | `pio device monitor -e cardputer-adv` | No reboot loop or fatal service failure | Pending | |

## System shell and controls

| ID | Test | Procedure | Expected | Result | Notes |
|---|---|---|---|---|---|
| SYS-01 | Boot Home | Power on or reset | Home appears; Keyboard card is selected; Keyboard app is not yet open | Pending | |
| SYS-02 | Launcher | Use Fn+`,` and Fn+`/`, then Enter | Selection wraps through Keyboard, GPS, Weather, and Settings; selected app opens | Pending | |
| SYS-03 | G0 short | From Keyboard, Settings, a confirmation page, and Home, tap G0 | Returns directly to Home without also opening Quick Settings | Pending | |
| SYS-04 | G0 long | Hold G0 for at least 600 ms in Home, Keyboard, and Settings | Quick Settings opens once; release does not trigger Home | Pending | |
| SYS-05 | Quick controls | Adjust brightness/volume, toggle BLE, close with Backspace | Hardware changes immediately; changed values survive restart | Pending | |
| SYS-06 | Settings diagnostics | Open Settings > System > Diagnostics | Reset reason, heap data, and recent bounded events are visible | Pending | |
| SYS-07 | Keyboard localization | Switch between English and Chinese, then observe Bluetooth off, advertising, pairing, connected, and error screens | Product state, explanation, and footer switch language; pairing digits and modifier names stay literal; no clipping or missing glyphs | Pending | |

## Initial pairing and reconnect

| ID | Test | Procedure | Expected | Result | Notes |
|---|---|---|---|---|---|
| BLE-01 | Passkey display | With no bond, open Keyboard | `PAIRING` and a six-digit code appear on Pocket Deck | Pending | |
| BLE-02 | macOS pairing direction | Select `Pocket Deck` on the Mac and enter Pocket Deck's displayed code in the Mac dialog | Pairing succeeds without typing the code on Cardputer | Pending | |
| BLE-03 | Secure connection | Wait after pairing | Keyboard shows `CONNECTED` and encrypted BLE HID status | Pending | |
| BLE-04 | Reboot reconnect | Restart Pocket Deck, wait on Home, then open Keyboard | Same `Pocket Deck` identity reconnects automatically; no new code or macOS Forget action is needed | Pending | |
| BLE-05 | Single-host policy | While the first bond exists, attempt pairing from another computer | Unknown host is rejected; existing bond remains usable | Pending | |

## Keymap and report behavior

Use a disposable local text field or keyboard event viewer on the paired Mac.
Do not use a shell, password field, chat, or document containing important data.

| ID | Test | Procedure | Expected | Result | Notes |
|---|---|---|---|---|---|
| KEY-01 | Plain keys | Test representative letters, digits, Space, Tab, Enter, Backspace | Standard US key meanings arrive once per physical transition | Pending | |
| KEY-02 | Punctuation | Test `` ` - = [ ] \\ ; ' , . / `` without Fn | Each produces its ordinary US punctuation key | Pending | |
| KEY-03 | Control | Hold Ctrl with a test key in an event viewer | Left Control modifier is reported | Pending | |
| KEY-04 | Option | Hold Opt with a test key | Left Option modifier is reported | Pending | |
| KEY-05 | Command | Hold Alt with a test key | Left Command modifier is reported | Pending | |
| KEY-06 | Shift | Hold Shift with a letter | Left Shift modifier is reported and screen modifier indicator follows hold/release | Pending | |
| KEY-07 | Fn arrows | Test Fn+`;`, Fn+`,`, Fn+`.`, Fn+`/` | Up, Left, Down, Right are reported in that order | Pending | |
| KEY-08 | Function endpoints | Test Fn+`1` and Fn+`=` | F1 and F12 are reported | Pending | |
| KEY-09 | Escape | Test Fn+`` ` `` | Escape is reported | Pending | |
| KEY-10 | Caps Lock | Test Fn+Tab twice | Caps Lock toggles on and back off | Pending | |
| KEY-11 | Forward Delete | Place the caret before text and press Fn+Backspace | Character after the caret is deleted | Pending | |
| KEY-12 | Held-key release | Hold a letter/modifier, observe repeat or down state, then release | Key remains logically held while physical key is held and releases cleanly; no stuck key | Pending | |
| KEY-13 | Six-key rollover boundary | Test a safe multi-key chord and then release all keys | Up to six non-modifiers plus modifiers report; release clears every key | Pending | |
| KEY-14 | Foreground privacy gate | Leave Keyboard for Home/Settings and press ordinary keys | No ordinary key reaches the Mac outside Keyboard | Pending | |

## Disconnect, recovery, and destructive actions

| ID | Test | Procedure | Expected | Result | Notes |
|---|---|---|---|---|---|
| REC-01 | Disconnect input drop | Settings > Bluetooth > Disconnect, then type in Keyboard | Link drops; keys pressed while disconnected are discarded | Pending | |
| REC-02 | Reconnect all-up | Hold a key during reconnect, then release it and press again | Held key is not replayed; first accepted state is all keys up; next press works | Pending | |
| REC-03 | Quick Settings release | Hold a key in Keyboard and open Quick Settings with G0 | Mac receives all-keys-up; overlay navigation never leaks as HID | Pending | |
| REC-04 | Forget confirmation cancel | Select Forget host, then Backspace | Bond remains and reconnect still works | Pending | |
| REC-05 | Forget and re-pair | Select Forget host, confirm with Enter, then pair again | Old bond is removed, a new code appears, and the Mac can pair without flash erase | Pending | |
| REC-06 | Restart confirmation | Select Restart, cancel once, then confirm | Cancel has no effect; confirm performs a clean software restart | Pending | |
| REC-07 | Factory reset | Change Quick Settings, save Wi-Fi profiles, ensure a bond exists, confirm Factory reset | App settings, all Wi-Fi profiles, and BLE bond clear; restart returns defaults and requires fresh pairing | Pending | |
| REC-08 | Post-reset identity | Compare the advertised device before and after Factory reset | Name/base device identity remains `Pocket Deck`; only settings/bond state reset | Pending | |

## Result summary

| Category | Status | Blocking issue / follow-up |
|---|---|---|
| Automated preflight | Pending | |
| System shell | Pending | |
| Pairing and reconnect | Pending | |
| Keymap | Pending | |
| Recovery and reset | Pending | |

A failed row should include the smallest reproducible sequence plus the newest
relevant on-device diagnostic lines.
