# LD2451 protocol notes

Based on the Hi-Link **serial protocol V1.03 (2024-07-01)** and real captured UART
reports. These concise implementation notes are not a replacement datasheet. See
[references](references.md) for manufacturer downloads and the separate electrical manual.

UART uses 115200 baud and the implementation uses 8N1. The reviewed protocol
explicitly gives one stop bit/no parity; 8 data bits were a bring-up assumption
supported by the captures. Multi-byte fields are little-endian.

Target reports have header `F4 F3 F2 F1`, two-byte payload length, payload, and
trailer `F8 F7 F6 F5`. Captured no-target reports use zero-length payloads. Populated
payloads contain a count, alarm byte and five bytes per target: angle (raw minus
128 degrees), distance in metres, raw direction, whole-km/h speed and raw SNR.
Validate length, count, fields and trailer; no checksum is specified. Buffer-read
timestamps are not measurement timestamps, and reported bearings/ranges are not
independently validated.

The protocol's direction table and worked example conflict. Preserve raw values.
Current policy provisionally accepts byte 1, supported by prompted toward captures
and approaching-only settings readback. Away-only capture contained empty reports;
it does not establish the meaning of populated byte-0 reports. No lane or stable
target identity is inferred. Host-side 750 ms freshness is independent of the
radar's configurable no-target delay.

Command/ACK envelopes use `FD FC FB FA`, two-byte length and `04 03 02 01` trailer.
The query engine only enters configuration, reads detection/sensitivity/version and
exits. It checks matching command, status and length, with bounded cleanup on timeout.
The firmware-version ACK length example is inconsistent; version was not obtained
on the tested device. A request-shaped echo is not a successful ACK. Readback values
are configuration, not calibration or guaranteed range/delay.

The `.bin` fixtures contain raw radar reports; JSON companions retain read boundaries
and decoded fields. Console `.log` excerpts are separately labelled. Synthetic warning
logs must never be used as measured-speed evidence.
