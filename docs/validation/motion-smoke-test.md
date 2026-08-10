# Pocket Deck MOTION Hardware Smoke Test

Target firmware: `0.9.6` on M5Stack Cardputer Adv with the built-in BMI270.

MOTION / 运动 uses an accelerometer and gyroscope only. It has **no
magnetometer, compass, heading, or absolute-orientation claim**. Do not treat a
successful compilation, native test, or screen render as physical sensor proof.
Do not upload the intermediate MOTION-only build: upload the final integrated
three-app firmware only after its required features pass. Do not erase flash,
clear NVS/bonds, format TF, or record raw axes, gestures, or user input.

## Implemented behavior to verify

- Three pages: `LIVE / 实时`, `LEVEL / 水平`, and `ACTIVITY / 活动`. Tab or Fn
  Left/Right changes pages; Backspace/G0 returns Home.
- LIVE shows accel X/Y/Z in `g` and gyro X/Y/Z in `deg/s`. LEVEL shows
  gravity-derived, smoothed roll/pitch; Enter makes the present attitude zero
  only for the current session. ACTIVITY shows STILL/MOVING/SHAKE, `|a|`, `|w|`,
  and the session acceleration-deviation peak; Enter resets that peak to the
  current value.
- While foreground, the service attempts one complete accel+gyro sample no more
  often than every 20 ms (50 Hz); normal non-MEDIA screen rendering is at most
  every 33 ms (up to 30 Hz). Exiting MOTION removes its IMU resource request.
- `|a| = sqrt(ax² + ay² + az²)`, deviation = `abs(|a| - 1)`, and
  `|w| = sqrt(gx² + gy² + gz²)`. STILL needs deviation `<0.08 g` and gyro
  `<10 deg/s`; SHAKE needs deviation `>=0.45 g` or gyro `>=180 deg/s`.
  SHAKE is immediate and latches for 500 ms; other state changes need five
  consecutive candidate samples.
- Diagnostics report IMU initialization/resource state only; no raw samples,
  axes, gestures, or activity stream are logged. Missing IMU shows
  `IMU UNAVAILABLE / IMU 不可用`; no complete sample shows
  `WAITING FOR SAMPLE / 等待传感器数据` while navigation remains usable.

## Build and native evidence

These results validate the current source, not physical hardware behavior.

| ID | Command | Expected | Result | Evidence |
|---|---|---|---|---|
| MOTION-BUILD-01 | `scripts/test-native.sh` | Native checks pass | Passed | `PASS: 1855 checks` |
| MOTION-BUILD-02 | `pio run -e cardputer-adv` | Target builds | Passed | RAM `104824` bytes; flash `2135813` bytes |

## Physical Cardputer Adv checks

Mark a result only after observing it on the physical device. All rows start
pending; no hardware success is claimed by this checklist.

| ID | Test | Expected | Result | Notes |
|---|---|---|---|---|
| MOTION-01 | Open `MOTION / 运动` from Home | Three pages navigate and localized titles/hints fit; Backspace and G0 return Home | Pending | |
| MOTION-02 | Axis response: slowly orient the Cardputer along each physical axis | LIVE accel/gyro X/Y/Z respond with plausible signs and units; no compass/heading is shown | Pending | |
| MOTION-03 | Level zero: hold a stable chosen attitude on LEVEL, press Enter, then move and return | Roll/pitch read near zero at the chosen attitude, change when tilted, and remain session-only after restart | Pending | |
| MOTION-04 | Stationary stability: leave the device still for at least several seconds | ACTIVITY settles to STILL without rapid flicker; normal small noise does not spuriously SHAKE | Pending | |
| MOTION-05 | Gentle continuous movement | ACTIVITY becomes MOVING only after five candidate samples and returns to STILL only after five still candidates | Pending | |
| MOTION-06 | Shake/latch: cross either documented shake threshold, then stop | SHAKE appears immediately and remains visible for at least 500 ms before non-shake hysteresis resumes | Pending | |
| MOTION-07 | Peak reset: create a peak, press Enter on ACTIVITY, then repeat movement | Session peak resets to the current deviation and rises again only when exceeded | Pending | |
| MOTION-08 | Unavailable/no-sample behavior (only when safely reproducible) | Missing IMU shows unavailable; incomplete sample shows waiting; all pages/Home controls remain responsive | Pending | |
| MOTION-09 | Exit/resource isolation: enter MOTION, then Home and another app | IMU is active only in MOTION; after exit no raw-motion events appear, and BLE/Wi-Fi/GPS/LoRa/MEDIA behavior remains intact | Pending | |
| MOTION-10 | Regression: run relevant launcher, localization, and resource-isolation paths after MOTION use | No reset, hang, display corruption, lost BLE bond, Wi-Fi-profile loss, GPS/LoRa failure, MEDIA regression, or unexpected background IMU activity | Pending | |

Retain only categorical diagnostics when investigating a failure. Do not add raw
motion streams to TF or serial logs; report approximate observed behavior
without sensitive user context.
