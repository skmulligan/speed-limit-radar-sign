# Configuration

Settings are compile-time constants. Edit, rebuild and upload the explicit intended
environment. There is no runtime configuration file, web UI or persistent settings API.

| Setting | Location | Current value |
| --- | --- | --- |
| Warning threshold | `firmware/include/warning_policy.h`, `Config::limitMph` | 20 MPH, strictly above before rounding |
| Release hysteresis | Same, `hysteresisMph` | 1 MPH; release at/below 19 MPH |
| Warning phases | Same, `numberMs`, `flashMs`, `messageMs`, `halfFlashMs` | 1000, 2000, 2000, 250 ms |
| Freshness | `firmware/include/speed_policy.h`, `kExpiryMs` | 750 ms |
| Eligible targets | Same, `State::update` | Fastest raw direction-1 target; provisional approaching interpretation |
| Brightness timing | `firmware/src/warning_bench.cpp`, `kPulseUs`, `kRowUs` | Continuous: 200 µs pulse, minimum 2000 µs row interval |
| Startup marker | Same, `kStartupCheckMs` | 3000 ms |
| UART | Same, `radar.begin` | 115200 8N1, RX GPIO17, TX disabled |
| Display pins | Same, GPIO constants | Data 11, clock 12, latch 10, ENABLE 9, row A 4, row B 5 |

Nominal enable duty is 10%; four-phase scanning gives 2.5% nominal per-pixel time
duty. IO/loop overhead reduces actual duty. Neither number is calibrated optical
brightness. Increasing brightness requires renewed loaded-power/thermal checks.

Speed is reported in whole km/h and displayed as rounded MPH; this is not a
measurement-accuracy claim. Display glyphs support 0–99, with the parser's accepted
speed range limiting current input. Threshold comparisons use unrounded speed.

Some serial banner strings contain literal default values (20 MPH / 750 ms). If
you customize settings, update those diagnostic strings and corresponding test
expectations so the logs stay accurate. Diagnostic drivers have separate pin/timing
constants; changing one environment does not update all of them automatically.

The sign does not set radar configuration. Historical readback reported approaching-only,
minimum 1 km/h, distance setting 100 m and no-target delay 1 s; those are settings,
not measured range/delay. Use the read-only query diagnostic to inspect another radar.
Do not silently change direction handling until controlled captures resolve semantics.
