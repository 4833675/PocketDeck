# Pocket Deck Sony Remote Hardware Smoke Test

Target: **Sony KD-65X9100H** and M5Stack Cardputer Adv running the final
integrated three-app firmware. REMOTE / 遥控器 is a fixed Sony-TV transmitter,
not a universal remote.

**Do not upload this intermediate build.** The final integrated three-app
firmware will be uploaded later. Do not erase flash, clear NVS or BLE bonds,
format TF, or change TV settings merely to make a check pass.

## Scope and status boundary

Every mapped press requests one initial Sony frame plus two repeats. D-pad, OK,
and TV Back use Sony device `151` / 15-bit frames. Return, Power, Home, Input,
Mute, Volume Down, and Volume Up use device `1` / 12-bit frames.

`SENT / 已发送` means only that the local IR library call completed. It does
**not** mean that the TV received, acknowledged, or acted on the command. This
firmware has no IR receive path, learning mode, other-brand profile, or
press-and-hold repeat behavior. Record physical TV observation separately from
the local status.

## Supported controls

| Cardputer control | Sony KD-65X9100H action |
|---|---|
| Fn + `;` | D-pad Up |
| Fn + `,` | D-pad Left |
| Fn + `.` | D-pad Down |
| Fn + `/` | D-pad Right |
| Enter | OK |
| Backspace | TV Back |
| Bare `` ` `` | Return |
| `p` | Power |
| `h` | Home |
| `i` | Input |
| `m` | Mute |
| `-` | Volume Down |
| `=` | Volume Up |
| G0 tap | Exit to Pocket Deck Home |

## Automated preflight

These checks validate source and build integration only. They never constitute
evidence that the Cardputer transmitted IR or that the TV received it.

| ID | Command | Expected | Result | Evidence |
|---|---|---|---|---|
| SONY-BUILD-01 | `scripts/test-native.sh` | Native checks pass | Passed | `PASS: 1340 checks`; source only. |
| SONY-BUILD-02 | `pio run -e cardputer-adv` | Cardputer Adv target builds | Passed | RAM `91096` bytes; flash `2115145` bytes; source only. |
| SONY-BUILD-03 | `git diff --check` | No whitespace errors | Passed | Final documentation/report check; no output. |

## Physical Sony KD-65X9100H checks

Keep every row `Pending` until observed on the physical Cardputer and exact TV.
For every command row, first note `SENT / 已发送` or a local error, then observe
the TV independently. Each listed press must request one initial frame plus two
repeats; do not use a held key as a substitute for repeated presses.

| ID | Test | Procedure | Expected TV observation | Result | Notes |
|---|---|---|---|---|---|
| SONY-01 | D-pad Up | Open REMOTE, point at the TV, press Fn + `;` once. | Selection/focus moves up when a current screen permits it. | Pending | |
| SONY-02 | D-pad Left | Press Fn + `,` once. | Selection/focus moves left when a current screen permits it. | Pending | |
| SONY-03 | D-pad Down | Press Fn + `.` once. | Selection/focus moves down when a current screen permits it. | Pending | |
| SONY-04 | D-pad Right | Press Fn + `/` once. | Selection/focus moves right when a current screen permits it. | Pending | |
| SONY-05 | OK | Press Enter once on a selectable TV item. | The focused item is accepted/opened. | Pending | |
| SONY-06 | TV Back | Press Backspace once from a TV subpage. | The TV returns one UI level. | Pending | |
| SONY-07 | Return | Press the bare backtick key once (without Fn). | The TV performs its Return action. | Pending | |
| SONY-08 | Power | With tester approval for the state change, press `p` once. | TV power state changes as expected. | Pending | Restore the prior state if appropriate. |
| SONY-09 | Home | Press `h` once. | The TV opens Home. | Pending | |
| SONY-10 | Input | Press `i` once. | The TV opens or advances Input selection. | Pending | Do not alter a source unintentionally. |
| SONY-11 | Mute | Press `m` once while audible program material is playing. | Audio mute changes state. | Pending | Restore the prior state if appropriate. |
| SONY-12 | Volume Down, repeated presses | Press `-` three separate times, waiting for each observable update. | Volume decreases in three observable steps; each press sent one initial frame plus two repeats. | Pending | Start from a safe audible level. |
| SONY-13 | Volume Up, repeated presses | Press `=` three separate times, waiting for each observable update. | Volume increases in three observable steps; each press sent one initial frame plus two repeats. | Pending | Restore a safe level. |
| SONY-14 | G0 exit | Tap G0 once from REMOTE. | Pocket Deck returns to Home; no TV command is assigned to G0. | Pending | |
| SONY-15 | IR off after exit | After SONY-14, enter another app and inspect the IR state only by an available non-invasive indicator/diagnostic. | REMOTE has released the IR resource; unrelated app input does not emit a Sony command. | Pending | Do not infer this from `SENT`. |
| SONY-16 | MEDIA / audio regression | After exiting REMOTE, start, pause, and stop a known MP3 through MEDIA. | Audio controls and playback remain responsive; no IR side effect is observed. | Pending | Do not format TF or expose track names. |
| SONY-17 | BLE regression | After exiting REMOTE, use Keyboard with the bonded Mac. | BLE remains connected/responsive and Remote keys do not leak as HID. | Pending | Do not record typed content. |
| SONY-18 | Wi-Fi regression | After exiting REMOTE, use Settings or Weather to scan/connect with an existing profile. | Wi-Fi remains responsive and profiles persist. | Pending | Do not record SSIDs or credentials. |
| SONY-19 | GPS regression | After exiting REMOTE, open GPS and navigate its pages. | GPS UI and existing data acquisition remain responsive. | Pending | Do not record precise coordinates. |
| SONY-20 | LoRa regression | With Cap antenna attached, open LORA without transmitting unless authorized. | LORA reaches its normal state or clear error without a reset/hang. | Pending | Do not transmit without antenna/location authorization. |
| SONY-21 | MOTION regression | After exiting REMOTE, open MOTION and move across its pages. | MOTION UI remains responsive; no unexpected IR activity is observed. | Pending | Preserve the existing MOTION checklist's pending rows. |
| SONY-22 | SSH regression | After exiting REMOTE, use an existing SSH host. | SSH connection and terminal remain responsive; no terminal content is logged. | Pending | Do not expose host or terminal content. |

## Result summary

| Area | Status | Blocking issue / follow-up |
|---|---|---|
| Automated preflight | Pending | Native/build/diff evidence is not hardware evidence. |
| Sony command set | Pending | Requires physical Sony KD-65X9100H observation per row. |
| IR release after G0 exit | Pending | Verify REMOTE resource is inactive after exit. |
| Cross-app regression | Pending | MEDIA/audio, BLE, Wi-Fi, GPS, LoRa, MOTION, and SSH remain unobserved. |
