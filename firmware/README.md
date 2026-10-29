# Choose your firmware

Both editions target the recorded ESP32-S3-N16R8, LD2451 and one red P10 panel.
They are independent PlatformIO projects; install only one on a board at a time.

| Edition | Intended use | Normal target | Maintenance |
| --- | --- | --- | --- |
| [Legacy — standalone](legacy/README.md) | Speed display and SLOW / DOWN, without logging or app integration | `continuous_radar` | Frozen pre-logging snapshot |
| [Current — logging + iOS](current/README.md) | Speed display, on-device event journal, BLE live view and iPhone history | `tracking` | All future development |

From the repository root, after installing the [pinned tools](../README.md):

```sh
python -m platformio run -d firmware/legacy -e continuous_radar
python -m platformio run -d firmware/current -e tracking
python firmware/legacy/tools/run_host_tests.py
python firmware/current/tools/run_host_tests.py
```

Each project defaults to `diagnostic`, a USB-only memory/serial test. Always name
the intended environment explicitly. The remaining environments are staged
bring-up tools; `continuous_radar` in current is a comparison target, not a third
maintained product. [iOS setup](../ios/README.md) applies only to current firmware.
Neither edition requires a cloud service or internet during operation.

Legacy carries its own headers, drivers and pinned configuration; it imports no
current code. See its [snapshot provenance](legacy/PROVENANCE.md). Shared curated
test inputs at `tests/fixtures/` retain their captured/synthetic labels.

The edition guides cover configuration, isolated USB upload and diagnostics.
Switching firmware does not require erasing the chip or formatting storage.
Preserve any existing history before changing firmware. Legacy does not access
the tracking journal; do not use an erase/format command as part of switching.
Software checks do not validate speed accuracy, timing under load or endurance.
