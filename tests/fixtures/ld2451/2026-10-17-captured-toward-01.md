# Prompted toward-only walking capture

Real hardware capture, not synthetic. The test operator reported readiness for walking toward
a stationary radar, then stopping. Exact movement timing, target identity,
true speed/distance and execution of the instructed movement were not separately
confirmed after capture. The association with toward movement is provisional.

Original full capture is retained privately and is not required to use this fixture.
Twenty-second capture, `radar_receive-v1`, GPIO17 RX, 115200 8N1, radar RX
unconnected. Kit USB power; previous loaded measurement 4.96 V. Panel/battery off.
Radar settings/firmware version not queried. Serial opening reset the ESP32;
radar power remained on. Capture timestamps are MCU buffer-read times.

Binary is the exact concatenated RAW bytes without corrections: 154 bytes,
14 valid envelopes. First two frames each contain one target: speed
3 km/h, distance 2 m, raw direction 1, alarm 1. Angles decode to -1 and +3 degrees,
SNR bytes 96 and 128. Timestamps 636 and 777 ms. Remaining twelve reports are
empty, beginning at 3564 ms. No UART errors or RAW sequence gaps observed.

Expected parser output: two one-target events then twelve no-target events.
This supports, but does not conclusively establish, direction byte 1 as toward;
the away-only comparison and configuration readback remain outstanding.
Not an accuracy or target-loss-time measurement.

SHA-256: `4768e73a364696f118b0840af84c9baf3ed1e0cce19693fd437203802c66a38f`.
