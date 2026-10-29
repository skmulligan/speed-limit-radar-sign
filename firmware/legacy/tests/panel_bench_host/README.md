# Panel bench host simulation

This deterministic fake Arduino runtime tests firmware logic without hardware.
It is synthetic evidence, not captured GPIO timing or validation of a panel.
Fake time advances only through delays; real interrupt/IO overhead is not modeled.

From the repository root:

```sh
mkdir -p tmp/firmware-host-tests/legacy/manual/panel-bench-host
c++ -std=c++17 -Wall -Wextra -I firmware/legacy/tests/panel_bench_host firmware/legacy/tests/panel_bench_host/check.cpp -o tmp/firmware-host-tests/legacy/manual/panel-bench-host/check
./tmp/firmware-host-tests/legacy/manual/panel-bench-host/check
```

Checks startup without a test, continuing four-phase row cycling, 128 shifted
bits per latch, one contrasting bit per test phase, requested 50-us enable
pulses, a three-second test deadline that repeated commands cannot extend,
explicit stop, overlong-command rejection, alternative polarity configuration,
timeout arithmetic across millis() rollover, and v2 constant-ENABLE low/high
probes with identical sparse data and automatic three-second completion.
V3 additionally checks a one-to-one candidate packing of all 512 coordinates,
the 48-pixel asymmetric orientation mark and per-phase payload counts, unchanged
50-us pulses, and a ten-second mapping deadline that repeats cannot extend.
Packing consistency is not evidence that the physical panel uses that packing.

V4 checks the fixed synthetic “20” against an independent glyph raster, its
120-pixel count and per-phase payloads, unchanged 50-us pulses, explicit stop,
and a ten-second deadline that another pattern command cannot extend.

Standalone synthetic battery-display checks:

```sh
c++ -std=c++17 -Wall -Wextra -I firmware/legacy/tests/panel_bench_host firmware/legacy/tests/panel_bench_host/battery_check.cpp -o tmp/firmware-host-tests/legacy/manual/panel-bench-host/battery-check
./tmp/firmware-host-tests/legacy/manual/panel-bench-host/battery-check
./tmp/firmware-host-tests/legacy/manual/panel-bench-host/battery-check cancel
./tmp/firmware-host-tests/legacy/manual/panel-bench-host/battery-check stop
./tmp/firmware-host-tests/legacy/manual/panel-bench-host/battery-check rollover
```

These check delayed start, ten-second fixed-20 duration, per-phase pixel counts,
50-us pulses, no automatic repeat, restricted commands, cancellation before
startup, explicit stop during display, and millis rollover. They do not validate
physical startup, battery supply, or screen illumination.

For the first daylight step, compile that same test with
`-DDAYLIGHT_PANEL_BENCH` and run all four invocations. Its independent expected
values are 15 seconds dark startup, 20 seconds fixed display, and 200-us pulses.
The standard panel and 50-us battery-panel checks must also continue to pass.
