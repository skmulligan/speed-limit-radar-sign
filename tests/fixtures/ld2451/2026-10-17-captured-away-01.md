# Prompted away-only walking capture

Real capture, not synthetic. The test operator reported readiness for walking away from a
stationary radar, then stopping. Execution, exact motion timing, true speed,
distance, and field of view were not independently observed or confirmed after
capture. Absence of target reports does not prove a direction filter.

Original full capture is retained privately; only the curated excerpt is published.
`radar_receive-v1`, GPIO17 RX, 115200 8N1, radar RX disconnected. Kit USB power;
previous loaded supply 4.96 V. Panel battery removed. Radar settings unqueried.
COM opening reset the ESP32; radar remained powered.

Exact concatenation of 14 consecutive RAW records, 140 bytes. All frames are
`F4 F3 F2 F1 00 00 F8 F7 F6 F5`, valid zero-length no-target envelopes. Nineteen
status heartbeats, zero UART errors, no sequence gaps. Expected parser output:
14 no-target events and no speed/direction values. JSON preserves timestamps.

Toward-only trial had direction byte 1 reports; this away-only trial has no
populated frames, so byte 0/receding semantics are still untested. Approaching-only
configuration is a hypothesis to check by reading settings, not a conclusion.
SHA-256: `463dbdcd32d144658e659338d6d4e5fd002f66f20dea9c0ecc4819d48a4742b5`.
