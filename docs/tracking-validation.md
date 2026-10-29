# Tracking setup and validation status

This guide applies only to [current firmware](../firmware/current/README.md).
Legacy standalone validation is recorded in [prototype validation](validation.md).

## Recorded evidence as of 2026-10-29

- Earlier tracking firmware: owner-reported phone pairing, phone-set clock,
  walking-speed display/clearing on the panel and app, and an event in History
  after sync. History survived app reopening; disconnected detections downloaded
  after reconnect, and repeat sync did not duplicate episodes.
- Subsequent power-cycle checks were reported qualitatively successful; exact
  counts and timestamp comparisons were not recorded.
- Tracking v2: isolated COM-USB startup and mounting of existing journal data
  passed. The console stop command was acknowledged. Peripherals were disconnected.
- These observations do not establish combined radar/display/BLE behavior on v2.
  Measured timing under flash/BLE load, RTC operation, vehicle-speed accuracy,
  sustained power/thermal behavior and two-hour endurance remain outstanding.

The repository reorganization changes packaging only. Host tests, builds and
simulator checks are software verification, not additional bench validation.
No runtime history, flash backup or private console capture is distributed.

## First setup and upgrades

1. Follow the [root isolation procedure](../README.md#build-and-flash): disconnect
   battery, external ESP32 headers, panel/JIN and radar adapter before COM USB.
2. Build and upload the explicit `tracking` target as described in the current
   guide. Monitor startup and `status` with powered loads disconnected.
3. Preserve existing storage before initialization. For an uninitialized/unmounted
   filesystem whose contents may be discarded, `init-storage` formats it and
   creates a new log identity. Never use initialization, full-chip erase or
   filesystem upload as a routine upgrade step. Existing partitions are retained.
4. Issue `pair`, connect the app to Street Speed and enter the serial passkey.
   Choose Set clock from this iPhone, then Sync now. See [iOS setup](../ios/README.md).
5. Unplug USB before restoring verified wiring with battery removed. Start with
   short attended radar/display checks. `stop` blanks the display; `live` resumes
   using fresh input. There is no synthetic mode in the logging target.

Without a valid RTC, calendar time is unknown after complete power removal until
set by the phone. Monotonic logging continues. Unsynced records are never silently
reclaimed; full storage pauses logging while the radar/display continue.

## RTC addition

Selected implementation reference: [Adafruit DS3231 STEMMA QT, product 5188](https://www.adafruit.com/product/5188),
with a CR1220 coin cell. No purchase or installation has occurred. Use that exact
breakout's [pinout](https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout/pinouts)
and [published schematic](https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout/downloads).
Do not substitute a generic charging RTC module without another hardware review.

| Signal | Planned connection |
| --- | --- |
| VIN | ESP32 3V3 |
| GND | Common logic ground |
| SDA | GPIO6 |
| SCL | GPIO7 |
| Other RTC pins | Unconnected |

The breakout's I2C pullups go to VIN, so use 3.3 V here. Firmware uses I2C address
0x68 at 100 kHz, validates BCD/date/24-hour format, rejects the oscillator-stop
flag and unsupported century, and stores UTC. Dates 2024–2099 are supported.
See the [DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf).
The firmware works with unknown calendar time until the clock is valid; it never
uses compile time as a date. A phone-set clock remains usable for this boot even
if the RTC write fails, with `rtcHealthy=false`.

Before installation, verify the delivered board labels/revision and GPIO6/7
availability on the actual assembled board, check clearances/mounting and add
measured RTC current to the power budget. After setting from the phone, verify
readback, full main-power removal with the coin cell present, and restart date.
Then test missing RTC and removed/depleted backup battery behavior. None of
these checks has yet passed on hardware.

## Outstanding acceptance checks

- Measure panel row/pulse timing and radar-to-visible response while flash writes,
  BLE bonding, live viewing and bulk sync occur. Inspect UART discard counters.
- Confirm stale, malformed, missing and delayed data clear both display and app.
- Exercise power interruption during append/flush/ACK, full unsynced storage,
  recovery gaps, repeated sync and durable phone acknowledgement.
- Test phone permissions, disconnection, suspension, bond replacement and restart.
- Validate RTC backup/time quality, speed/direction, loaded rails, cutoff switching,
  thermal behavior, weather protection and endurance before broader deployment.

Record observations against the exact firmware revision. Episodes describe
detection activity, not identified vehicles or calibrated vehicle counts.
