# Current — logging + iOS firmware

Use the [root setup instructions](../../README.md) to install the pinned Python tools.
The target is `esp32-s3-devkitc-1` with explicit **16 MB quad flash / 8 MB octal PSRAM**
overrides for the recorded N16R8 board. The board profile does not identify a clone's
power circuitry or header order. PlatformIO Espressif32 6.12.0 and Arduino-ESP32
2.0.17 (package 3.20017.241212) are pinned. COM upload/monitor speed is 115200.

## Normal sign build

This is the actively maintained edition. It keeps the speed display and warning,
records detection episodes in an ESP32 LittleFS journal, and uses BLE for live
viewing and explicit foreground sync to the [iPhone app](../../ios/README.md).
The phone stores history in its local SQLite database. No account, Wi-Fi,
internet connection or cloud backend is required. The sign works without a
connected phone; if unsynced storage fills, logging stops while display continues.

```sh
python -m platformio run -d firmware/current -e tracking
```

After following the [root USB isolation procedure](../../README.md), upload with:

```sh
python -m platformio run -d firmware/current -e tracking -t upload --upload-port YOUR_PORT
python -m platformio device monitor --port YOUR_PORT --baud 115200
```

Inspect `status` and preserve existing data before any initialization. Only for
an uninitialized/unmounted filesystem whose contents may be discarded, issue
`init-storage`; it formats storage and creates a new log identity. Never use it
as a routine update step. The partition layout is unchanged. Pair with `pair`,
enter the serial passkey on the phone, and choose Set clock from this iPhone.
See [commissioning](../../docs/tracking-validation.md) before restoring loads.

Startup shows STARTING for three seconds. `stop` clears/disarms the display;
`live` discards old input and resumes. This target has no synthetic demo mode.
The display uses a timer scanner; storage and BLE/control use separate tasks.
The 750-ms expiry remains, with an additional conservative 20-ms UART stall
guard. Its physical response under combined flash/BLE load remains unmeasured.

An optional DS3231 provides battery-backed calendar time. The selected reference
is Adafruit DS3231 STEMMA QT 5188 with CR1220; installation and backup operation
remain unvalidated. The proposed interface is 3.3 V, SDA GPIO6, SCL GPIO7.
Use the [documented module pinout](../../docs/tracking-validation.md#rtc-addition),
not an assumed connector order. Without a valid
RTC, phone time works for the current powered session; after complete power
removal calendar time is unknown until set again. Monotonic logging continues.

Change compile-time settings using [configuration](../../docs/configuration.md).
Protocol/storage details are in [tracking protocol](../../docs/tracking-protocol.md).
Observed results and outstanding checks are in [validation](../../docs/tracking-validation.md).

## Choose a diagnostic

All environments remain available. Replace ENVIRONMENT with a name below:

```sh
python -m platformio run -d firmware/current -e ENVIRONMENT
```

Add `-t upload --upload-port YOUR_PORT` only after the appropriate isolation steps.

| Environment | Purpose and activation |
| --- | --- |
| `tracking` | Normal maintained sign: logging, BLE and iOS; `status`, `stop`, `live`, `pair`, `forget-phone`, `init-storage`. |
| `diagnostic` (default) | USB-only memory/serial check; disconnect peripherals. `r` repeats memory checks, `?` shows configuration. |
| `radar_receive` | Passive GPIO17 radar receiver; TX disabled; prints RAW records. Use separately powered radar with common ground, panel/battery off. |
| `display_io` | Static GPIO/shifter checks; panel must be disconnected. `low`, `high`, `1`–`6`, `?`; high states expire after 60 s. |
| `panel_bench` | Manual 50 µs panel trials: `test`, `map`, `digits`, `stop`. Polarity probes are advanced bring-up tools. |
| `battery_panel_bench` | Automatic synthetic 20 after 5 s dark; 10 s display, 50 µs pulses. |
| `daylight_panel_bench` | Automatic synthetic 20 after 15 s dark; 20 s display, 200 µs pulses. |
| `radar_query` | Explicit `query` reads settings/version via GPIO18; `status`, `stop`; no persistent settings writes. Panel/battery off. |
| `live_bench` | Explicit `live`, bounded 20 s live-speed trial, 50 µs pulses; `status`, `stop`. |
| `warning_bench` | Explicit `live` or synthetic `demo-warning`, bounded 20 s, 50 µs pulses. |
| `battery_bench` | Automatic 20 s live trial after 5 s dark, 50 µs pulses. |
| `daylight_radar_bench` | Automatic 20 s live trial after 15 s dark, 200 µs pulses. |
| `daylight_radar_diagnostic` | 15 s dark, 3 s synthetic TEST, 20 s live, 200 µs pulses. |
| `continuous_radar` (comparison) | 3 s STARTING, then indefinite live operation, 200 µs pulses. |

Warning-based environments accept `status`, `stop`, `live` and `demo-warning`.
Stop an active run before changing mode. In continuous mode, `live` restarts live
reception without repeating STARTING; `demo-warning` is still a bounded 20-second
synthetic sequence with TEST identification. A reset restores automatic startup.

Do not treat synthetic numbers as measurements. Keep panel power disconnected
while establishing a serial session. A powered multi-supply bench setup requires
separately checked power paths; the battery-only wiring guide does not authorize
connecting COM USB to the assembled, battery-powered sign.

## Optional host tools

All scripts accept `--help`. `pyserial` is included in the root requirements file;
no private `PYTHONPATH` or temporary esptool installation is required.

```sh
python firmware/current/tools/capture_diagnostic.py --port YOUR_PORT --output logs/current/diagnostic.log
python firmware/current/tools/capture_radar.py --port YOUR_PORT --output logs/current/radar.log --seconds 20
python firmware/current/tools/warning_console.py --port YOUR_PORT --output logs/current/warning.log
```

Use each tool only with its matching diagnostic and setup. The diagnostic capture
sends `r`; the radar capture passively records data. The persistent `panel_console`,
`live_console`, `warning_console` and `query_console` tools are **POSIX-only** because
they use `select` on terminal/serial file descriptors. Native Windows users can use
PlatformIO's serial monitor for manual newline-terminated commands instead; account
for reset-on-open behavior. Capture tools are not physically verified on every OS.
Console log files must be new; existing files are not overwritten. Full logs stay local.

## Software regression checks

From the repository root, with a C++17 compiler available:

```sh
python firmware/current/tools/run_host_tests.py
```

The runner compiles 15 host configurations and executes 50 scenarios. `CXX` can
select another compiler. Output goes to ignored `tmp/firmware-host-tests/current/`. The
live test needs the four captured `.bin` fixtures under `tests/fixtures/ld2451/`.
Other host cases use synthetic UART/GPIO/time. Per-suite READMEs document coverage.

To compile all 14 MCU environments without uploading:

```sh
python -m platformio run -d firmware/current -e diagnostic -e radar_receive -e display_io -e panel_bench -e battery_panel_bench -e daylight_panel_bench -e radar_query -e live_bench -e warning_bench -e battery_bench -e daylight_radar_bench -e daylight_radar_diagnostic -e continuous_radar -e tracking
```

## Code map

- `include/radar_reports.h`: framing and report validation.
- `include/speed_policy.h`: eligible target selection, conversion and freshness.
- `include/warning_policy.h`: threshold, hysteresis and animation state.
- `include/panel_mapping.h`, `speed_digits.h`, `warning_text.h`: panel mapping/raster.
- `src/warning_bench.cpp`: shared warning, battery and continuous hardware integration.
- `include/radar_query.h`: bounded read-only query transaction engine.

- `src/tracking.cpp`: maintained radar, storage and BLE task integration.
- `include/tracking/`: records, journal, clock, transport and timer scanner.

Keep parsing/policy independent of hardware drivers. Preserve fresh-data checks
through warning animations, stops, UART errors and timer rollover.
