# Radar query host checks

Synthetic tests of the exact transaction engine used by the MCU. They do not
verify physical UART, radar settings, or real ACK formats.

```sh
mkdir -p tmp/radar-query-host
c++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/radar_query_host/check.cpp -o tmp/radar-query-host/check
./tmp/radar-query-host/check
```

Checks exact allowlisted enter/read/end request packets, no startup request,
matching command/status/length requirements, unrelated ACKs, fragmented input,
malformed length/trailer recovery, the firmware-version length discrepancy,
repeat rejection, cancellation, bounded timeout cleanup and millis rollover.
