# Read-only query capture

Real hardware capture, not synthetic. The test operator confirmed radar USB powered after
GPIO18-to-RX connection and `radar_query-v1` installation. Battery/panel off.
Original full capture is retained privately. Companion log is
an exact byte slice from the first QUERY_STARTED through the second TX_DETACHED,
with a final newline added. Intermediate RX/status/host lines are retained.
SHA-256 of excerpt: `26f5b3c49aad0a85136d19ed7d37820923efd9c7877ebfc565fb8d30bfca551c`.

First attempt received an enter-request-shaped echo, not the expected successful
0x01FF ACK. It timed out, sent end configuration and received successful 0x01FE.
Cause of that initial response is unknown. One bounded retry followed.

Retry received successful matching ACKs for enter, detection and sensitivity:
- Detection bytes `64 01 01 01`: distance setting 100 m, direction selection 1
  (approaching-only per V1.03), minimum speed 1 km/h, no-target delay 1 s.
- Sensitivity bytes `02 00 00 00`: two valid detections; threshold selection 0
  means default level 4 per V1.03, reserved bytes zero.
- Firmware command 0x00A0 had no response before the 2-second experimental timeout;
  firmware version remains unknown. Do not treat the full query as successful.
- Cleanup received successful end-config ACK. TX detached, normal empty reports
  resumed. Both attempts ended with QUERY_FAILED but CONFIG_EXIT_CONFIRMED.

UART error counter was 1 while radar unpowered, rose to 2 during power-up before
query, and remained 2 through both transactions and the subsequent idle status.
Thus no additional errors were observed during the query; do not call the whole
power-up session error-free. No persistent-setting/write/reset/baud commands sent.
The parser labels any valid command envelope ACK; the first request-shaped echo
is NOT a valid acknowledgement. Interpret command/status explicitly.

Values are readback settings, not measured accuracy, guaranteed detection range,
or calibrated delay. Direction setting agrees with toward detections/away silence;
raw receding report semantics are still untested. RX chunks can be concatenated;
ACK lines duplicate received bytes and must not be concatenated into RX again.
