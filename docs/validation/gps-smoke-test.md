# Pocket Deck GPS Hardware Smoke Test

Target firmware: `0.4.0` or later

Hardware: M5Stack Cardputer Adv + Cap LoRa-1262

Result values: `PASS`, `FAIL`, or `BLOCKED`

Do not treat a successful build as proof of satellite reception. Test the fix
rows outdoors with the Cap's ceramic GNSS antenna facing open sky.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| GPS-01 | Boot with Cap attached | Home loads normally; BLE remains available | Pending | |
| GPS-02 | Launcher | GPS card appears between Keyboard and Weather | Pending | |
| GPS-03 | UART stream | GPS 3/4 `RX CHARS` rises continuously | Pending | |
| GPS-04 | NMEA validity | `CHECKSUM OK` rises; ERR does not dominate | Pending | |
| GPS-05 | UTC before fix | UTC time/date appears when supplied by receiver | Pending | |
| GPS-06 | Satellite acquisition | Satellite count becomes greater than zero outdoors | Pending | |
| GPS-07 | Position fix | State becomes `FIX`; latitude/longitude update | Pending | |
| GPS-08 | Altitude/quality | Altitude, HDOP, mode, and quality display plausible values | Pending | |
| GPS-09 | Page 4 moving | On `GPS 4/4`, move with a valid fix. | Large speed in `KM/H` and course degrees plus compass point update plausibly. | Pending | |
| GPS-10 | Page 4 stationary | On `GPS 4/4`, remain stationary with valid motion fields. | Speed/course are shown only while valid; a near-zero speed remains plausible and no previous moving value is falsely retained. | Pending | |
| GPS-11 | Page 4 invalid motion | While `GPS 4/4` is visible, make either speed or course invalid while the app remains running. | The invalid field shows `--`; the other field follows its own validity. | Pending | |
| GPS-12 | Page 4 stale motion | Allow GPS data/fix to become stale while `GPS 4/4` is visible. | Both motion displays show `--`, not cached speed or course. | Pending | |
| GPS-13 | Page navigation | Left, Right, and Tab navigate all four pages and wrap `1 -> 4 -> 1`. | Pending | |
| GPS-14 | Background parsing | Leave GPS, wait, return; counters/data continued updating | Pending | |
| GPS-15 | BLE coexistence | Keyboard pairs/types normally while GPS receives in background | Pending | |
| GPS-16 | Missing stream | Disconnect Cap and restart; GPS reports `NO DATA`, system remains usable | Pending | |
| GPS-17 | Stream interruption | Disconnect after data reception; state changes to `NO STREAM` | Pending | |

Record the page-3 `RX CHARS`, `CHECKSUM OK`, and `CHECKSUM ERR` values whenever
reporting a GPS problem. Do not include precise coordinates in shared logs unless
location disclosure is intentional.
