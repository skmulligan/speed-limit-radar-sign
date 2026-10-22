# First prompted walking capture

Real hardware capture, not synthetic. The test operator reported readiness for a stationary
radar test, walking briskly toward it and then away. Exact movement timing,
actual speed/distance, and target identity were not independently measured.

Original full capture is retained privately; only the curated excerpt is published.
LD2451 TX to ESP32 GPIO17, `radar_receive-v1`, 115200 8N1; radar RX disconnected.
Kit USB adapter power; previous reported loaded voltage 4.96 V. Battery/panel off.
Radar configuration/firmware version not queried. Thirty-second capture.

Binary concatenates exact RAW bytes without correction. 24 records,
282 bytes, 24 valid report envelopes, of which
18 are empty. Every populated payload has
one target, alarm 1, direction byte 1, distance field 2 m. Speeds reported
are 3 km/h in five frames and 7 km/h in one. One angle decodes as +86 degrees;
preserve it as reported, not an established physical bearing. The other angles
are -3, -19, 0, +9, +9 degrees. No UART errors or RAW sequence gaps observed.

Populated records are 0–5, buffer-read timestamps 913–3422 ms. First empty
report is at 6210 ms. This gap does not establish target-loss delay because
movement timing and configured delay were not measured. Companion JSON retains
RAW boundaries/timestamps and independent envelope/field decoding.

Expected parser events: six single-target reports followed by eighteen empty
reports. Direction semantics, actual speed accuracy, target identity, and
coverage remain unvalidated. No current vehicle speed may be inferred from
these historical data. SHA-256: `27a3d93573bd81f2c973d7fb65cbc9b31275bf8c516111f891f19713bda094d1`.
