# First live display bench checks

Run from the repository root:

```sh
mkdir -p tmp/firmware-host-tests/legacy/manual/live-bench-host
c++ -std=c++17 -Wall -Wextra -pedantic -I firmware/legacy/tests/live_bench_host firmware/legacy/tests/live_bench_host/check.cpp -o tmp/firmware-host-tests/legacy/manual/live-bench-host/check
./tmp/firmware-host-tests/legacy/manual/live-bench-host/check
```

The parser consumes all four real captured no-target/walking/toward/away fixtures.
Synthetic cases check fragmented input, bad length/count/trailer/fields, resync,
fastest eligible approach selection, MPH rounding, immediate no-target/opposite
clearing, 750-ms expiry and timestamp rollover. Numeric 20 matches the previously
bench-tested fixed raster exactly; supported displayed values 0–75 render within
panel bounds. Remaining glyph shapes have not been individually bench-tested.

The actual firmware loop runs under fake GPIO/time/UART to verify dark startup,
RX-only UART, cleared pre-arm sample, short enable pulses, expiry without new
frames, explicit stop, no-target clearing, backlog/UART-error clearing and a
20-second trial that repeated commands cannot extend. Fake timing excludes
physical interrupts/IO overhead and is not evidence of actual panel timing,
brightness, current, or speed accuracy. The repeated GPIO stub is local to this
bench test and does not change the earlier panel test harness.
