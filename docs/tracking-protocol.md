# Tracking protocol and storage v1

Implemented 2026-10-27; experimental. Earlier tracking firmware passed limited
functional checks; combined-load checks on tracking v2 remain pending. See
[commissioning and acceptance](tracking-validation.md) and [iOS setup](../ios/README.md).
The radar UART protocol remains the documented LD2451 V1.03 parser input. The
following is our ESP32/app protocol, not a vendor protocol.

## Event semantics

An episode starts with the first eligible real report. Selection remains the
highest whole-km/h speed among raw-direction-1 detections, with the first entry
winning ties. This direction interpretation remains provisional. The selected
report index now accompanies the live data. No-target/ineligible-only reports,
invalid input, UART faults/backlog, 750-ms expiry and stop close the episode.
A reboot leaves an interrupted episode; the phone marks open episodes from a
previous boot incomplete when the new session arrives.

An episode is continuous detection activity, not a vehicle ID. Overlap can merge
vehicles, and a gap can split one vehicle. One-second buckets are aligned to the
episode's first sample using 64-bit monotonic milliseconds; missing/invalid data
is never represented as a zero-speed sample. Each bucket stores selected-speed
min/max/sum/count, observation span, first-sample offset and maximum simultaneous
target count. Mean is sample-weighted, not a time-weighted vehicle speed.

Native speed remains integer km/h. MPH = km/h / 1.609344. The current strictly
above-20-mph threshold corresponds to integer reports of at least 33 km/h.
Episode duration is last valid observation minus first, not closure timeout.
Single-observation episodes legitimately have zero observed duration.

## Journal

LittleFS uses the existing `default_16MB.csv` data partition, mounted at
`/littlefs`. No partition migration, OTA implementation, automatic format or
microSD is introduced. Files are `/s-<16 hexadecimal first-sequence digits>.bin`.
Explicit initialization of an unmounted filesystem rotates the device/log
identity before formatting, preventing sequence reuse from colliding with an
older phone database. No automatic recovery operation formats the partition.
A segment holds up to 256 records, including a self-describing session record.
Keep at least 128 KiB free after allocating the next segment. Exact retention
must be measured; the earlier 32-byte sizing estimate does not apply to v1.
An 80-byte bucket per active second gives a theoretical 12.3 hours in 3.375 MiB
before episode/session metadata, reserve and filesystem overhead.

In tracking v2, a dedicated storage task owns all application filesystem/NVS
work. Radar sends records without waiting, through a static FIFO admitting 64
normal records plus eight reserved control entries. BLE/control requests pass
through radar into the same FIFO: a SYNC's position is its ordering barrier.
Only storage assigns journal sequence numbers. `fflush` and `fsync` run when
dirty at a one-second worker deadline, and before exposing a new sync snapshot
or confirming a clock anchor. Failure latches storage fault. The
implementation targets at most two seconds of uncommitted observations (one
partial bucket plus one flush interval) during healthy operation; power-cut
bench tests must establish the actual loss bound.

Queue residence counts against this budget. Status exposes maximum commit age
and overruns rather than implying that a periodic timer guarantees durability.
Queue saturation pauses aggregation and reserves a separate loss notice. Storage
drains earlier accepted records; after an append failure it discards subsequent
queued observation records. Once the radar acknowledges the pause and the FIFO
is empty, storage commits a Gap before allowing a new episode. The existing
phone Gap handling marks open episodes incomplete. No new record kind or v1
field meaning is introduced, and loss counts are attempted records, not cars.

Records are fixed 80-byte little-endian structures; encode/decode is explicit,
not a compiler struct dump:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | Magic `SDG1` (`0x31474453`) |
| 4 | 1 | Schema version, 1 |
| 5 | 1 | Kind: Session=1, Start=2, Bucket=3, End=4, Clock=5, Gap=6 |
| 6 | 2 | Time quality: unknown=0, RTC=1, phone=2 |
| 8 | 8 | Record sequence, increasing across boots of this log identity |
| 16 | 8 | Random boot/session ID |
| 24 | 8 | Monotonic milliseconds since ESP32 boot |
| 32 | 8 | Episode number within boot; 0 for non-episode records |
| 40 | 8 | Signed UTC Unix milliseconds; 0 if unknown |
| 48 | 28 | Seven unsigned 32-bit kind-specific values |
| 76 | 4 | IEEE CRC-32 of bytes 0–75 |

Kind-specific values:

- Session: firmware build revision 2 (1 for the commissioned v1), threshold 20 MPH, expiry 750 ms,
  bucket width 1000 ms, segment capacity 256 records, record size 80, reserved 0.
- Start: threshold 20 MPH, selection-policy version 1, remaining values 0.
- Bucket: observed span ms, min km/h, max km/h, sum km/h, sample count,
  maximum report target count, first-sample offset within bucket. `mono` is the
  bucket boundary; last observation = mono + first offset + observed span.
- End: reason, low/high 32 bits of last valid monotonic observation, remaining 0.
  Reasons: no eligible target=1, invalid=2, UART/backlog=3, expiry=4, stop=5,
  reboot=6 (phone-derived), storage=7.
- Clock: zero values; `mono` and `utc` form a new anchor.
- Gap: cause (logging unavailable=1, corrupt/torn stored record=2), dropped-record
  count, low/high 32 bits of logging-pause start when known, remaining 0.

Only real UART input enters this target. Existing explicitly synthetic diagnostic
firmware is preserved separately and never feeds the logger. Synthetic wire
fixtures have separate provenance notes and are not normal app history.

Recovery scans segment names and physical slots; a partial/corrupt slot is sent
as an explicit Gap at that sequence, never a reconstructed speed. Recovered
segments are sealed; new writes use a new filename. Never acknowledge beyond
committed data. Acknowledgements persist in NVS and can be repeated. Reclaim only
fully acknowledged sealed segments when space is needed. An unsynced full log
stops recording, exposes `full`, and resumes with a gap after a successful sync
makes space available. Device status and serial report lost-record attempts;
these are not counts of missed vehicles.

## BLE service

All UUIDs share suffix `-7065-6564-6465-746563746f72`:

| Prefix | Role | Properties |
| --- | --- | --- |
| `73640001` | Service | Discovery |
| `73640002` | Status and command response | Notify |
| `73640003` | Commands | Write with response |
| `73640004` | Live snapshots | Notify |
| `73640005` | Historical batches | Notify |

The three notification subscriptions and command characteristic require
MITM-protected encrypted access. Commission one bonded owner phone using the
explicit USB `pair` command (120-second window; passkey printed to serial).
`forget-phone` removes bonds; physical console access is required. First pairing
requires the documented isolated USB/power setup. This code uses the Arduino
2.0.17 bundled Bluedroid library; pairing has been observed on earlier tracking firmware; full lifecycle and
current-v2 load testing remain pending.

Every characteristic message uses the same fragmentation:

- Header: message ID u16, total payload-plus-CRC length u16, fragment offset u16.
- At default MTU, up to 14 bytes follow the header (20-byte ATT value). Commands
  use this baseline. Notifications use the negotiated MTU minus three bytes,
  capped at a 244-byte ATT value; the service requests a 247-byte MTU. Both ends
  accept variable fragment lengths. GATT-rejected fragments retry without
  advancing the offset rather than silently skipping bytes under congestion.
- The assembled message ends with IEEE CRC-32 of the preceding payload.
- Maximum assembled size 2048 bytes. A new offset-zero fragment starts a message;
  missing/out-of-order fragments invalidate the assembly. IDs are per stream and
  may wrap. Protocol/schema version is carried inside status/live/records.

Commands are UTF-8 ASCII, then framed as above:

| Command | Meaning |
| --- | --- |
| `STATUS` | Device identity, boot, storage, clock, error and retention status |
| `PING <nonce>` | Status response `pong`, echoed nonce and device monotonic time |
| `TIME <UTC-ms>` | Set phone time and attempt DS3231 write; append a clock anchor |
| `SYNC <request-id> <cursor>` | Replace any pending transfer; flush/fix a committed high-water mark and fetch the next batch |
| `NEXT <request-id> <cursor>` | Fetch another batch under the same request and high-water mark |
| `ACK <cursor>` | Acknowledge durable contiguous phone storage; no immediate erase |

A history payload is high-water u64, current oldest retained sequence u64,
request ID u64, then 0–16 records. The phone ignores batches from superseded
requests, imports and updates its cursor atomically, then sends
ACK and NEXT. Empty batches terminate an up-to-date transfer. Retention gaps
before `oldest` are explicitly stored in the phone database. Different bytes for
a duplicate key are rejected rather than silently replaced. A new SYNC resumes
from the phone's durable cursor after transfer timeout/disconnection.

Tracking v2 performs these commands asynchronously. Replies and transfer state
carry internal connection generations; obsolete replies after disconnect are
discarded. An applied TIME anchor is still persisted if the connection ends
while it is queued. Records carry their production-time UTC/quality snapshot;
storage never restamps old queued data using a newer clock. `time_set` requires
successful append and flush; `time_storage_failed` leaves the usable phone time
visible but does not claim persistence. `busy` indicates bounded control capacity
was unavailable; `storage_fault` refuses a new sync snapshot after flush failure.
If a completion cannot fit in the bounded reply queue, `response_lost` clears
the pending command rather than blocking later commands indefinitely; inspect
status or restart sync to establish the operation's result.

The console stop command has a separate coalescing mailbox. It bypasses the
normal pending-command gate, disarms radar and blanks the panel before emitting
`COMMAND stopped` on the console. This acknowledgement does not promise durable
storage: radar queues a separate FIFO flush barrier, retrying when the queue is
full. Existing pending sync/ACK/TIME completions retain their original tickets.
Earlier queued live commands are superseded by stop; new live commands are
accepted only after stop is applied and the normal command slot is available.

BLE commands retain their original ingress connection generation through
dispatch. TIME rechecks that generation and authorization after RTC I2C work;
a stale result cannot be retagged for a new connection, applied to the radar clock
or queued as a new anchor. An RTC transaction already underway may have written
the chip; it cannot be rolled back on disconnect. This differs from a TIME
already applied by radar, whose accepted Clock record still persists normally.

Status and live payloads are JSON. UInt64 identities/sequences are strings to
avoid JSON-number precision loss; monotonic millisecond values are numbers.
Status includes `v`, `response`, `token`, `device`, `boot`, `mono`, `oldest`,
`high`, `ack`, `transferHigh`, `used`, `capacity`, `storage`, `clockQuality`,
`rtcHealthy`, `dropped`, `uartErrors`, `invalidBytes`, `backlogs`, `commandDrops`.
`rtcHealthy` means successful initialization/write this session, not continuous
battery-voltage monitoring.

Optional v2 diagnostic status fields are `serviceGapUs`, `publishUs`, `appendUs`,
`flushUs`, `queueHigh`, `queueAgeMs`, `commitAgeMs`, `durabilityOverruns`, `stalls`.
They are software maxima/counters, not calibrated wire-to-panel latency. Existing
v1 clients ignore these additions. Queue age includes control requests; commit
age starts at the earliest observation represented by pending records.

Live includes `v`, `boot`, `sequence`, `sampleMono`, `sentMono`, `state`,
`selected`, `warning`, `alarm`, `targets`. Each target is
`[decodedAngleDegrees, distanceMeters, rawDirection, speedKmh, rawSNR]`.
Selection is a zero-based report index, -1 if no eligible selection. States are
`starting`, `valid`, `no_eligible`, `no_target`, `expired`, `fault`, `stopped`.
No persistent target identity or geometric trajectory is transmitted.

The v2 radar task bounds UART service and partial-frame retention at 20 ms. After
an overrun it discards buffered input and clears the sign. Accepted reports use
the preceding service time minus that retention budget as a conservative sample
bound; expiry may therefore be earlier than in v1. The aggregation tick uses the
same time bound so bucket boundaries cannot run ahead of delayed samples. This
does not establish actual radar acquisition time. The scanner rejects frame
publication if the UART fault generation changed during rendering, and rechecks
the current deadline before each ENABLE pulse.

Status responses precede live/history; history gets a bounded share of outgoing
fragments. Live coalesces to the newest state at up to 10 Hz without replacing
an in-flight message except on a state transition. This is a maximum scheduling
rate, not measured radio throughput. Historical bytes have their own channel and
never update the live view.

The phone maps device monotonic time using PING send/receive bounds. Mapping the
device's ping timestamp to the phone's send time deliberately overestimates age
by up to the round-trip time. It rejects reordered reports, changed boots and
invalid snapshots; expires at 750 ms and discards mappings after ten seconds.
Pings repeat every four seconds in foreground. Suspension/reconnection clears
live state until a new mapping and fresh report arrive.

## Phone database and Watch boundary

SQLite schema version 1 has devices/cursors, original record blobs, episodes,
buckets and explicit gaps. `HistoryStore` is a Swift actor; imports use a single
`BEGIN IMMEDIATE` transaction with `synchronous=FULL`. No acknowledgement is sent
on a failed import. Unknown times can be backfilled only from an anchor in the
same boot and are labeled reconstructed. Clock corrections do not change record
identity or monotonic durations.

The shared Swift package supports iOS/macOS/watchOS and exports the protocol,
`LiveRadarSnapshot`, `LiveClock`, `DetectionEpisodeSummary` and database models.
A future watch companion can reuse these with an iPhone WatchConnectivity relay.
No watch app, background alert delivery, direct Watch BLE, cloud service, Wi-Fi
transport, erase command or radar reconfiguration is implemented in v1.
