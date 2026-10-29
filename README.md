# Street Speed Reminder

A battery-powered radar speed display built with an **HLK-LD2451**, an
**ESP32-S3-N16R8**, and one **32 × 16 red P10 LED panel**. It shows selected-target
speed in MPH and flashes the speed followed by **SLOW / DOWN** above the threshold.

This is the firmware and wiring companion for a forthcoming Instructable.
The guided assembly article is not published yet. The diagrams describe this
prototype's connections, not a universal P10 pinout.

## Choose an edition

| Edition | Features | Maintenance |
| --- | --- | --- |
| [Legacy — standalone](firmware/legacy/README.md) | Speed display and warnings; no logging or app integration | Frozen pre-logging snapshot |
| [Current — logging + iOS](firmware/current/README.md) | Speed display, on-device event journal, BLE live view and iPhone SQLite history | Actively maintained; all future features |

Both operate without internet or a cloud backend. Current can log without a
connected phone, then sync in the foreground later. Without a valid optional RTC,
set time from the phone after complete power removal; undated monotonic logging
still works. See the [firmware selector](firmware/README.md) and [iOS setup](ios/README.md).

## How the sign behaves

Both normal targets display STARTING for three seconds, then listen indefinitely.
STARTING confirms display activity, not radar lock. They select the fastest
target with raw direction byte 1; direction interpretation remains provisional.
No eligible target means a blank display.

The warning starts strictly above 20 MPH using unrounded radar speed, and releases
at or below 19 MPH: one second of speed, two seconds flashing, then two seconds
of SLOW / DOWN. No-target reports, invalid data, UART faults and sample expiry
clear the reading. Current additionally bounds buffered-input age and uses a
timer-driven scanner. See [configuration](docs/configuration.md) for differences.

## Start here

| Task | Guide |
| --- | --- |
| Identify compatible components | [Hardware overview](docs/hardware.md) |
| Gather parts | [Complete BOM](hardware/protoboard/bom.md) |
| Wire and commission the assembly | [Protoboard guide](hardware/protoboard/README.md) |
| Set up iPhone live view and history (current only) | [iOS guide](ios/README.md) |
| Build, flash and run diagnostics | [Firmware guide](firmware/README.md) |
| Change speed threshold, timing or brightness | [Configuration](docs/configuration.md) |
| Diagnose blank display, resets or UART errors | [Troubleshooting](docs/troubleshooting.md) |
| Understand what has been checked | [Validation status](docs/validation.md) |
| Find datasheets and protocol notes | [References](docs/references.md) and [protocol notes](docs/protocol.md) |

## Build and flash

Use Python 3.10 or newer and PlatformIO Core. Commands below run from the repository
root. Choose one firmware edition for your board. Create a virtual environment (`python` may be named `python3` on your system):

```sh
python -m venv .venv
```

Activate it with `source .venv/bin/activate` on macOS/Linux, or
`.venv\Scripts\Activate.ps1` in Windows PowerShell. Then:

```sh
python -m pip install -r requirements-dev.txt
python -m platformio run -d firmware/legacy -e continuous_radar
python -m platformio run -d firmware/current -e tracking
python -m platformio device list
```

Before connecting COM USB, **remove the battery, disconnect all external ESP32
header connections, panel power/JIN, and the radar USB adapter**. Remove ESP-LINK
as well; that link alone does not establish isolation. Use the ESP32 board's
**COM** socket, not its native USB socket. Substitute the detected port for `YOUR_PORT`:

```sh
python -m platformio run -d firmware/legacy -e continuous_radar -t upload --upload-port YOUR_PORT
python -m platformio device monitor --port YOUR_PORT --baud 115200
```

The upload example selects legacy; for current use `-d firmware/current -e tracking`
with the same upload/port flags. Follow the [current setup](firmware/current/README.md)
for storage initialization and pairing; never erase/format as a routine update.

With peripherals isolated, verify the selected firmware banner and serial activity;
this does not test the panel or radar. Close the monitor and unplug USB before
restoring wiring with the battery still removed. Then perform the staged checks in
the wiring guide. Serial opening can reset the board and restart automatic operation.

Each edition defaults to `diagnostic`. Always specify `-e continuous_radar` for
legacy or `-e tracking` for current. No precompiled binaries are supplied.

## Development

A C++17 compiler (`c++`, or set `CXX`) and Python run the hardware-free tests:

```sh
python firmware/legacy/tools/run_host_tests.py
python firmware/current/tools/run_host_tests.py
```

The tests cover real captured packets and explicitly synthetic cases. They do not
establish physical accuracy, brightness, runtime or electrical safety. Firmware
parsing and display policy remain separate from GPIO/UART operations. See the
[maintenance conventions](docs/repository.md) before contributing.

## Prototype status

Current v2 passed isolated USB startup and existing-storage mounting on 2026-10-29.
Earlier tracking firmware had owner-reported live view/history sync checks.
Combined-load v2 timing and RTC operation remain pending; see
[tracking validation](docs/tracking-validation.md).


As of **2026-10-22**, the assembled protoboard has a reported five-minute functional
run without glitches/restarts and no noticeable heat by touch. Startup, walking-speed
display and subsequent clearing were observed. Earlier wiring passed short runs
through ten minutes and a reported 50-foot readability trial; those results do not
qualify this assembly for prolonged operation or every daylight condition.

Vehicle-speed accuracy, complete direction semantics, actual low-voltage cutoff
switching, final branch protection, measured thermal performance, weather protection
and two-hour battery endurance remain unresolved. This is a prototype driver reminder,
not a calibrated speed instrument. See the validation notes for evidence boundaries.

## License

Original project code and accompanying original documentation are [MIT licensed](LICENSE).
External dependencies and reference works retain their own terms; see
[third-party notices](THIRD_PARTY_NOTICES.md). Maintained as `skmulligan`.
