# Warning behavior software checks

Run from the repository root:

```sh
mkdir -p tmp/firmware-host-tests/legacy/manual/warning-bench-host
c++ -std=c++17 -Wall -Wextra -pedantic -I firmware/legacy/tests/live_bench_host firmware/legacy/tests/warning_bench_host/check.cpp -o tmp/firmware-host-tests/legacy/manual/warning-bench-host/check
./tmp/firmware-host-tests/legacy/manual/warning-bench-host/check
```

All inputs here are **synthetic**, including the UART packets and demo timeline.
The existing live bench fake Arduino runtime is reused. Checks cover unrounded
threshold/hysteresis, phase boundaries, updated numeric values, blanking on
expiry/empty sample in every phase, timestamp rollover, and text bounds. The
exact SLOW / DOWN raster is printed for visual review.

The real firmware loop is exercised for dark startup, RX-only UART, 20-second
bounded demo, input-source isolation, signal loss during message, later recovery,
empty reports, no repeated-command extension, mode-change sample clearing,
stop, malformed input, UART faults/backlog, stale live data and 50-us enable
pulses. Fake time excludes physical interrupt and IO overhead. This establishes
software behavior only; physical panel cadence/readability, current and radar
accuracy require separate observations.
