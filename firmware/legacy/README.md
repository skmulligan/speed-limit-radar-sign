# Legacy — standalone firmware

This edition is frozen; new development belongs to current firmware.
See [source provenance and checksums](PROVENANCE.md).

Use the [root setup instructions](../../README.md) to install the pinned Python tools.
The target is `esp32-s3-devkitc-1` with explicit **16 MB quad flash / 8 MB octal PSRAM**
overrides for the recorded N16R8 board. The board profile does not identify a clone's
power circuitry or header order. PlatformIO Espressif32 6.12.0 and Arduino-ESP32
2.0.17 (package 3.20017.241212) are pinned. COM upload/monitor speed is 115200.

## Normal sign build

```sh
python -m platformio run -d firmware/legacy -e continuous_radar
```

Follow the root guide's complete USB isolation procedure before uploading. During
battery-only use, continuous firmware displays STARTING for three seconds and then
runs live until power loss, reset or an explicit stop. It transmits no radar commands.
Fresh data is required after every start. The synthetic demo never starts automatically.

Compile-time controls are described in [configuration](../../docs/configuration.md).
Changes require rebuilding and uploading; there is no saved settings UI.

## Choose a diagnostic

All environments remain available. Replace ENVIRONMENT with a name below:

```sh
python -m platformio run -d firmware/legacy -e ENVIRONMENT
```

Add `-t upload --upload-port YOUR_PORT` only after the appropriate isolation steps.

| Environment | Purpose and activation |
| --- | --- |
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
| `continuous_radar` | 3 s STARTING, then indefinite live operation, 200 µs pulses. |

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
python firmware/legacy/tools/capture_diagnostic.py --port YOUR_PORT --output logs/legacy/diagnostic.log
python firmware/legacy/tools/capture_radar.py --port YOUR_PORT --output logs/legacy/radar.log --seconds 20
python firmware/legacy/tools/warning_console.py --port YOUR_PORT --output logs/legacy/warning.log
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
python firmware/legacy/tools/run_host_tests.py
```

The runner compiles ten host configurations and executes 28 scenarios. `CXX` can
select another compiler. Output goes to ignored `tmp/firmware-host-tests/legacy/`. The
live test needs the four captured `.bin` fixtures under `tests/fixtures/ld2451/`.
Other host cases use synthetic UART/GPIO/time. Per-suite READMEs document coverage.

To compile all 13 MCU environments without uploading:

```sh
python -m platformio run -d firmware/legacy -e diagnostic -e radar_receive -e display_io -e panel_bench -e battery_panel_bench -e daylight_panel_bench -e radar_query -e live_bench -e warning_bench -e battery_bench -e daylight_radar_bench -e daylight_radar_diagnostic -e continuous_radar
```

## Code map

- `include/radar_reports.h`: framing and report validation.
- `include/speed_policy.h`: eligible target selection, conversion and freshness.
- `include/warning_policy.h`: threshold, hysteresis and animation state.
- `include/panel_mapping.h`, `speed_digits.h`, `warning_text.h`: panel mapping/raster.
- `src/warning_bench.cpp`: shared warning, battery and continuous hardware integration.
- `include/radar_query.h`: bounded read-only query transaction engine.

Keep parsing/policy independent of hardware drivers. Preserve fresh-data checks
through warning animations, stops, UART errors and timer rollover.
