# Pocket Deck SSH Terminal Hardware Smoke Test

Target firmware: `0.9.6` on M5Stack Cardputer Adv.

Use a disposable or otherwise appropriate SSH target. Do not paste private key
material, typed commands, terminal output, hostnames, usernames, or IP addresses
into this checklist.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| SSH-01 | Open SSH Terminal | Host list appears; UI remains responsive | Pass | Host configured on device; UI remained responsive |
| SSH-02 | Press `N`, fill four fields, restart | New host survives restart | Pass | Record survived repeated firmware uploads and restarts |
| SSH-03 | Edit a host with `E` | Updated values persist | Pending | |
| SSH-04 | Delete a host with `D`, then cancel | Host remains | Pending | |
| SSH-05 | Confirm host deletion | Only selected host is removed | Pending | |
| SSH-06 | Try connecting with Wi-Fi off | Clear Wi-Fi error; no freeze | Pending | |
| SSH-07 | Connect to a host accepting the embedded public key | Shell prompt appears | Pass | v0.9.6 reached CONNECTED; shell-ready had 41,240 bytes free and a 23,540-byte largest block |
| SSH-08 | Type a simple command | Output is readable and input is not duplicated | Pending | |
| SSH-09 | Send Ctrl+C and Ctrl+D | Remote shell receives control bytes | Pending | |
| SSH-10 | Use Fn arrows and shell history | Direction sequences work | Pending | |
| SSH-11 | Open Fn+Tab quick commands | Selected command runs once | Pending | |
| SSH-12 | Produce more than one screen of output | Opt+Fn Up/Down scrolls local history | Pending | |
| SSH-13 | Drop and restore Wi-Fi in the SSH app | Wi-Fi-not-ready retries after recovery; other failures wait for Enter | Pending | |
| SSH-14 | Tap G0 during a session | Home opens and SSH disconnects | Pending | |
| COEX-SSH-01 | Keep BLE connected while using SSH | BLE recovers and other apps remain usable | Pending | |
| PRIV-SSH-01 | Inspect serial and TF diagnostics | No key, command, or terminal content appears | Pending | |
| RESET-SSH-01 | Factory reset | SSH hosts clear; firmware key remains compiled in | Pending | |
| LOCAL-SSH-01 | Switch English / Chinese across host list, editor, connect, disconnect, errors, retry, delete, history, and quick commands | Product prompts switch language; host values, commands, terminal stream, ANSI colors, and `NVS!` remain exact | Pending | |
| MEM-SSH-01 | Cold boot, enter SSH, connect three times, and inspect TF diagnostics | Every attempt reaches authentication/shell without `Out of memory`; SSH init has substantial headroom and ANSI/UI colors remain correct | Pending | One v0.9.6 connection passed: init 85,380 bytes free, transport-ready 43,840, shell-ready 41,240; two repetitions and full color sweep remain |
