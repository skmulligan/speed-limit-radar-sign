# Tracking host checks

With the root guide’s Python environment active, run `python firmware/current/tools/run_host_tests.py` from
the repository root. All inputs and platform adapters here are synthetic.

- `check.cpp`: episode aggregation, target selection, framing/CRC, golden wire
  fixture and monotonic wrap behavior.
- `journal_check.cpp`: real host file operations through the LittleFS adapter,
  incomplete-record recovery, retained sequence/ACK, injected flush/ACK failures
  and acknowledged-only reclamation. It does not simulate flash timing or wear.
- `calendar_check.cpp`: supported date and RTC encoding/validation.
- `runtime_check.cpp`: compiles the actual tracking task iteration functions
  with controlled hardware/storage/transport adapters. Scenarios cover storage
  delay/failure and queue saturation while radar progresses, recovery gaps,
  ordered sync barriers, clock changes, stale input, stop/live, publication
  races, bucket boundaries, completion overflow and threaded handoffs.
  Stop regressions include a pending storage request, saturated command/reply
  queues and re-arm ordering. Connection regressions cover disconnect/reconnect
  inside RTC I/O, normal TIME completion, and stale dispatch/queued commands.
- `panel_check.cpp`: compiles the real tracking panel header against simulated
  GPIO registers and timer calls. Faults injected during rendering/scanning
  cannot publish or illuminate an invalid frame; expired frames remain blank.

The runtime adapter supplies host mutexes/FIFOs and fake task timing. It does not
run the ESP-IDF scheduler, Bluetooth stack, physical UART or flash/cache behavior.
Panel delay hooks model interleavings, not electrical timing. Both are software
regressions; hardware acceptance remains in `docs/tracking-validation.md`.

The journal test creates and removes an isolated directory under the system
temporary directory. It uses POSIX `mkdtemp`; the full suite is verified on macOS.
