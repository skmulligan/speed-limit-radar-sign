# First captured no-target stream

Real hardware capture, not synthetic. Original full capture is retained privately and is not required to use this fixture.
LD2451 TX -> ESP32-S3 GPIO17; `radar_receive-v1`, 115200 8N1,
TX disabled. Blue kit adapter supplies radar; the test operator measured 4.96 V at VIN/GND.
Battery/panel power off. Module firmware version/settings not yet queried.

The binary concatenates the exact hex payload bytes from RAW records 0 through
13, preserving order without correction. It contains 140 bytes:
14 identical valid zero-length report envelopes, each
`F4 F3 F2 F1 00 00 F8 F7 F6 F5`. No reported targets and no speed values.
No record gaps or UART errors observed. Inter-record MCU buffer-read intervals
were 1393–1396 ms (mean 1394.31 ms); these are not
radar measurement timestamps or a guaranteed report interval.

Expected parser result: 14 no-target events, zero targets; clear any current
speed. This establishes observed empty-frame shape only, not populated report
fields, direction semantics, speed accuracy, or radar sensitivity.
SHA-256: `463dbdcd32d144658e659338d6d4e5fd002f66f20dea9c0ecc4819d48a4742b5`.
