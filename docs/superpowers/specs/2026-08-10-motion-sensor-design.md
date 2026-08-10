# MOTION Sensor App Design

Date: 2026-08-10
Target: Pocket Deck on M5Stack Cardputer Adv
Status: Approved for implementation

## Goal

Add a bilingual `MOTION / 运动` app that exposes the Cardputer Adv's built-in
BMI270 accelerometer and gyroscope without affecting SSH, MEDIA, BLE, Wi-Fi,
GPS, LoRa, or TF logging. The first release is an instrument and gesture
diagnostic, not an automatic system-control layer.

## Scope

The app has three pages:

1. `LIVE / 实时数据`
   - Acceleration X/Y/Z in g.
   - Angular velocity X/Y/Z in degrees per second.
2. `LEVEL / 水平仪`
   - Smoothed roll and pitch.
   - A bounded two-axis bubble visualization.
   - Enter stores the current roll and pitch as a temporary zero reference.
3. `ACTIVITY / 运动状态`
   - `STILL / 静止`, `MOVING / 移动`, or `SHAKE / 摇晃`.
   - Acceleration magnitude, gyro magnitude, and session peak.
   - Enter clears the session peak.

The BMI270 has no magnetometer. The app must not display a compass, magnetic
heading, or stationary north reference.

## Controls

- Tab or Fn+Left/Fn+Right: cycle pages.
- Enter: zero the level page or clear the activity peak.
- Backspace or G0: return Home.

All product-owned labels and error messages ship in English and Simplified
Chinese. Axis names, units, and numeric values remain literal.

## Architecture

Add an `ImuService`, a portable motion-state model, and a `MotionApp`.

`ImuService` owns M5Unified IMU access and publishes a fixed-size snapshot. It
initializes against the board-owned M5Unified instance, verifies that an IMU is
available, and samples only while its foreground resource is active. It does
not reinitialize the shared I2C bus during an app switch.

Add `RuntimeResource::Imu`; only `AppId::Motion` requests it. The service runs at
50 Hz while active and performs no polling outside the app. Quick Settings may
temporarily overlay the app without changing this resource profile.

The level calculation uses the M5Unified logical axes:

- roll = atan2(ay, az)
- pitch = atan2(-ax, sqrt(ay^2 + az^2))

Apply an exponential low-pass filter with alpha 0.2. The zero reference is
session-only and is cleared on reboot; this release does not write IMU offsets
to NVS.

The portable motion model uses acceleration deviation from 1 g and gyro
magnitude. Initial thresholds are:

- still: acceleration deviation below 0.08 g and gyro below 10 deg/s;
- shake: acceleration deviation at or above 0.45 g or gyro at or above
  180 deg/s;
- otherwise moving.

Still/moving transitions require five consecutive samples. Shake is latched for
500 ms so the label is readable. Hardware validation may establish that a
future release needs different thresholds; this release uses the values above.

## UI and launcher integration

Add `AppId::Motion` to the launcher carousel with glyph `IM`. The app uses the
existing indexed palette and shared localized font helpers. Rendering remains
at the standard 30 Hz while sampling remains at 50 Hz.

When the IMU is unavailable, the app shows `IMU UNAVAILABLE / IMU 不可用` and
remains responsive. It must not block startup or other apps.

## Diagnostics and privacy

Log only app/service lifecycle and initialization failure. Never log continuous
axis samples, derived orientation, motion history, or per-gesture events.

## Validation

- Native tests cover threshold boundaries, five-sample hysteresis, shake latch,
  peak reset, page cycling, and the IMU resource profile.
- Full firmware build must preserve the existing SSH heap budget.
- Hardware checks verify axis response, level zeroing, stationary stability,
  shake detection, app exit, and BLE/Wi-Fi/GPS/LoRa/MEDIA regressions.
- Hardware validation records no raw motion samples in project documents or
  diagnostics.

## Non-goals

- Compass or magnetic heading.
- Pedometer, fall alarm, background gesture shortcuts, or BLE air mouse.
- Persistent IMU calibration.
