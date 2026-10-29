# Battery-only trial software checks

All UART input is synthetic; these checks do not establish physical operation.
Run from the repository root:

```sh
mkdir -p tmp/firmware-host-tests/current/manual/battery-bench-host
c++ -std=c++17 -Wall -Wextra -pedantic -I firmware/current/tests/live_bench_host firmware/current/tests/battery_bench_host/check.cpp -o tmp/firmware-host-tests/current/manual/battery-bench-host/check
tmp/firmware-host-tests/current/manual/battery-bench-host/check
tmp/firmware-host-tests/current/manual/battery-bench-host/check rollover
tmp/firmware-host-tests/current/manual/battery-bench-host/check stop
```

The real firmware loop runs against the existing fake Arduino runtime. Checks
cover the five-second dark startup, discarded pre-arm samples/queued packets,
new live data, stale/error clearing, 50-us enable pulses, the 20-second deadline
even with continuing fresh input, no automatic restart, early stop cancellation,
and millis rollover. Also run `warning_bench_host` to check the unchanged manual
warning/demo behavior shared by this build.

Compile this same check with `-DDAYLIGHT_RADAR_BENCH` for the brighter live
variant and run the normal, rollover, and stop cases. It checks independent
expected values of 15-second startup delay and 200-us pulses, along with the
same 20-second live deadline and sample-clearing rules. The stop case runs past
the full startup-plus-trial period to verify that cancellation prevents output.

Combined display/radar diagnostic:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -I firmware/current/tests/live_bench_host firmware/current/tests/battery_bench_host/startup_check.cpp -o tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check
./tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check
./tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check rollover
./tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check stop-pending
./tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check stop-marker
./tmp/firmware-host-tests/current/manual/battery-bench-host/startup-check stop-live
```

Checks 15-second dark startup, independently counted TEST raster at 200-us
pulses for three seconds despite radar UART faults, rejection of competing
commands during TEST, fresh-data-only transition to a 20-second live window,
stale/error clearing, no automatic repeat and cancellation in each phase.
The marker is deliberately synthetic text, never a speed sample. These are
software simulations, not display illumination or radar validation.

Continuous live build:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I firmware/current/tests/live_bench_host firmware/current/tests/battery_bench_host/continuous_check.cpp -o tmp/firmware-host-tests/current/manual/battery-bench-host/continuous-check
for mode in normal rollover stop-pending stop-marker stop-live; do ./tmp/firmware-host-tests/current/manual/battery-bench-host/continuous-check "$mode"; done
```

Checks the independent full STARTING raster and scan counts, immediate startup,
three-second marker despite UART faults, fresh-only live transition, 65 seconds
of simulated continuous traffic, sparse long-elapsed clock jumps, millis wrap,
warning text, no-target/malformed/UART/backlog/expiry clearing, no revival of an
expired sample after a full clock wrap, stop in each phase, and a still-bounded
explicit synthetic demo. The long clock jumps are not a two-hour simulation
or physical endurance test. Run the older battery/startup/manual-warning tests
as regressions for the shared source.
