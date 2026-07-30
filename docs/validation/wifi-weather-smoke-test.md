# Pocket Deck Wi-Fi and Weather Hardware Smoke Test

Target firmware: `0.4.0` or later on M5Stack Cardputer Adv.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| UI-01 | Observe status bar radio labels while connected/disconnected/off | `WiFi` and `BT` use mint/amber/red respectively | Pending | |
| UI-02 | Observe status bar before and after NTP sync | Center changes from `--:--` to UTC+8 `HH:MM` | Pending | |
| UI-03 | Disconnect Wi-Fi after NTP sync | Clock continues for the current boot | Pending | |
| WIFI-01 | Enable Wi-Fi in Settings | State leaves OFF without disturbing BLE | Pending | |
| WIFI-02 | Scan networks | Nearby 2.4 GHz SSIDs appear, strongest first | Pending | |
| WIFI-02A | Leave the old saved AP unavailable, then press Tab in Scan networks | Scan starts instead of entering ERROR | Pending | |
| WIFI-03 | Enter password | Characters stay masked and never type on the Mac | Pending | |
| WIFI-04 | Connect | State becomes CONNECTED with plausible RSSI/IP | Pending | |
| WIFI-05 | Network info | IP, gateway, DNS, status, and NTP UTC are visible | Pending | |
| WIFI-06 | Save two networks, restart with both visible | Stronger saved network reconnects without a password | Pending | |
| WIFI-07 | Make the strongest saved network reject or time out | The next visible saved candidate is attempted | Pending | |
| WIFI-08 | Delete one entry under Saved networks | Only that profile is erased; the other remains | Pending | |
| COEX-01 | Type during scan/connect | BLE HID remains connected and responsive | Pending | |
| GPS-01 | Open GPS while Wi-Fi is connected | GPS stream and FIX continue | Pending | |
| WX-01 | Open Weather with Wi-Fi but no GPS fix | Screen requests a fresh GPS fix | Pending | |
| WX-02 | Open Weather with Wi-Fi and GPS fix | Current weather, high/low, sunrise/sunset appear | Pending | |
| WX-03 | Press Enter in Weather | A manual refresh completes without freezing UI | Pending | |
| WX-04 | Disconnect GPS after a successful forecast | Existing weather remains with an amber cached/GPS status | Pending | |
| WX-05 | Fail a refresh after a successful forecast | Existing weather remains with an update-error status | Pending | |
| RESET-01 | Factory reset | App settings, all Wi-Fi profiles, and BLE bond are erased | Pending | |
