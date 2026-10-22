# Street Speed Reminder

A standalone, battery-powered radar speed display built with an **HLK-LD2451**, an
**ESP32-S3-N16R8**, and one **32 × 16 red P10 LED panel**. It shows approaching-target
speed in MPH and flashes the speed followed by **SLOW / DOWN** above the configured
threshold. It runs locally without Wi-Fi, Bluetooth or an internet connection.

This repository is the firmware and wiring companion for a forthcoming Instructable.
The guided assembly article is not published yet. Start with the wiring guide below;
the diagrams describe this prototype's connections, not a universal P10 pinout.

## How it behaves

The `continuous_radar` build displays **STARTING** for three seconds, then listens
for radar reports indefinitely. STARTING confirms display activity, not a radar lock.
It selects the fastest target with raw direction byte 1; that direction interpretation
is provisional. No eligible target means a blank display.

The default warning begins **strictly above 20 MPH**, using the unrounded radar
speed, and releases at or below 19 MPH. The sequence is one second of speed, two
seconds flashing, then two seconds of SLOW / DOWN, repeating while appropriate.
No-target reports, invalid data, UART faults/backlog and a **750 ms** sample expiry
clear the reading. Old readings are never presented as a current vehicle speed.

## Start here

| Task | Guide |
| --- | --- |
| Identify compatible components | [Hardware overview](docs/hardware.md) |
| Gather parts | [Complete BOM](hardware/protoboard/bom.md) |
| Wire and commission the assembly | [Protoboard guide](hardware/protoboard/README.md) |
| Build, flash and run diagnostics | [Firmware guide](firmware/README.md) |
| Change speed threshold, timing or brightness | [Configuration](docs/configuration.md) |
| Diagnose blank display, resets or UART errors | [Troubleshooting](docs/troubleshooting.md) |
| Understand what has been checked | [Validation status](docs/validation.md) |
| Find datasheets and protocol notes | [References](docs/references.md) and [protocol notes](docs/protocol.md) |

## Build and flash

Use Python 3.10 or newer and PlatformIO Core. Commands below run from the repository
root. Create a virtual environment (`python` may be named `python3` on your system):

```sh
python -m venv .venv
```

Activate it with `source .venv/bin/activate` on macOS/Linux, or
`.venv\Scripts\Activate.ps1` in Windows PowerShell. Then:

```sh
python -m pip install -r requirements-dev.txt
python -m platformio run -d firmware -e continuous_radar
python -m platformio device list
```

Before connecting COM USB, **remove the battery, disconnect all external ESP32
header connections, panel power/JIN, and the radar USB adapter**. Remove ESP-LINK
as well; that link alone does not establish isolation. Use the ESP32 board's
**COM** socket, not its native USB socket. Substitute the detected port for `YOUR_PORT`:

```sh
python -m platformio run -d firmware -e continuous_radar -t upload --upload-port YOUR_PORT
python -m platformio device monitor --port YOUR_PORT --baud 115200
```

With peripherals isolated, verify the continuous-build banner and serial activity;
this does not test the panel or radar. Close the monitor and unplug USB before
restoring wiring with the battery still removed. Then perform the staged checks in
the wiring guide. Serial opening can reset the board and restart automatic operation.

The default PlatformIO environment remains `diagnostic`. Always specify
`-e continuous_radar` for the sign. No precompiled binaries are supplied.

## Development

A C++17 compiler (`c++`, or set `CXX`) and Python run the hardware-free tests:

```sh
python firmware/tools/run_host_tests.py
```

The tests cover real captured packets and explicitly synthetic cases. They do not
establish physical accuracy, brightness, runtime or electrical safety. Firmware
parsing and display policy remain separate from GPIO/UART operations. See the
[maintenance conventions](docs/repository.md) before contributing.

## Prototype status

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
