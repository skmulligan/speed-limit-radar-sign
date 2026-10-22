# Validation status — 2026-10-22

These are reported prototype observations, not independent certification. Hardware
observations below predate preparation of this public package. Software checks and
compilation do not add bench validation.

| Assembly / check | Recorded result and limit |
| --- | --- |
| Controller | ESP32-S3, 16 MB flash and 8 MB PSRAM identified; two six-pattern tests over a 7 MiB allocation passed. Not full memory or endurance qualification. |
| Earlier wiring | Static signal and cable-continuity checks, panel polarity/orientation, synthetic 20, live walking-speed display and clearing, and synthetic warning sequence observed. |
| Earlier battery assembly | Reported 1-, 5- and 10-minute functional runs, clearing and no noticeable warmth after ten minutes. No measured temperature/current study. |
| Earlier visibility trial | Display reported visible at 50 feet at 200 µs pulses; illumination and viewing angle unrecorded. Not an all-day readability result. |
| Current protoboard | Reported unpowered isolation/continuity checks, approximately 10 kΩ enable pull-downs, steady 3.3 V VCB and 5.17 V VCA/radar/panel measurements in their checked stages. No transient characterization. |
| Current protoboard operation | STARTING, subsequent blanking and walking-speed display/clearing observed. Five-minute run reported without glitches/restarts; nothing felt hot. Ten-minute check on this assembly remains pending. |
| Cutoff | Configured 18.0 V / 2.0 V hysteresis retained through a power cycle. Actual trip/reconnect not verified. |
| Radar query | Detection/sensitivity readback obtained; firmware-version request timed out. Raw receding semantics remain unresolved. |

## Still required

Reference vehicle-speed comparison and intended-direction checks; measured response
and clearing behavior; final fuse/wire/protection review; actual cutoff switching;
loaded current and temperature measurements; longer continuous runs and representative
two-hour endurance; completed enclosure, mounting and weather/readability assessment.

Repeat relevant checks after hardware or brightness changes. Do not transfer earlier
assembly results to new wiring as if it had completed the same run.

## Software evidence

`python firmware/tools/run_host_tests.py` runs 28 scenarios across ten host builds.
Tests include captured report parsing plus synthetic malformed input, freshness,
warning transitions, startup/stop handling and rollover. Curated fixture notes specify
which data were captured and which were synthetic. MCU builds verify compilation only.
