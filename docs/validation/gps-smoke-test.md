# Pocket Deck GPS Hardware Smoke Test

Target firmware: `0.2.0`  
Hardware: M5Stack Cardputer Adv + Cap LoRa-1262  
Result values: `PASS`, `FAIL`, or `BLOCKED`

Do not treat a successful build as proof of satellite reception. Test the fix
rows outdoors with the Cap's ceramic GNSS antenna facing open sky.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| GPS-01 | Boot with Cap attached | Home loads normally; BLE remains available | Pending | |
| GPS-02 | Launcher | GPS card appears between Keyboard and Settings | Pending | |
| GPS-03 | UART stream | GPS 3/3 `RX CHARS` rises continuously | Pending | |
| GPS-04 | NMEA validity | `CHECKSUM OK` rises; ERR does not dominate | Pending | |
| GPS-05 | UTC before fix | UTC time/date appears when supplied by receiver | Pending | |
| GPS-06 | Satellite acquisition | Satellite count becomes greater than zero outdoors | Pending | |
| GPS-07 | Position fix | State becomes `FIX`; latitude/longitude update | Pending | |
| GPS-08 | Altitude/quality | Altitude, HDOP, mode, and quality display plausible values | Pending | |
| GPS-09 | Motion data | Speed/course update when moving and remain plausible at rest | Pending | |
| GPS-10 | Page navigation | Left, Right, and Tab navigate all three pages | Pending | |
| GPS-11 | Background parsing | Leave GPS, wait, return; counters/data continued updating | Pending | |
| GPS-12 | BLE coexistence | Keyboard pairs/types normally while GPS receives in background | Pending | |
| GPS-13 | Missing stream | Disconnect Cap and restart; GPS reports `NO DATA`, system remains usable | Pending | |
| GPS-14 | Stream interruption | Disconnect after data reception; state changes to `NO STREAM` | Pending | |

Record the page-3 `RX CHARS`, `CHECKSUM OK`, and `CHECKSUM ERR` values whenever
reporting a GPS problem. Do not include precise coordinates in shared logs unless
location disclosure is intentional.
