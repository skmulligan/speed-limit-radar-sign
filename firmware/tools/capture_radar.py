"""Capture passive radar_receive logs; does not interpret target measurements."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--expect-idle", action="store_true",
                        help="Check disconnected receiver heartbeats, not radar operation")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    statuses = records = received = 0
    faults = []
    previous_sequence = None
    previous_ms = None
    with args.output.open("x", encoding="utf-8") as log:
        log.write(f"# UTC {datetime.now(timezone.utc).isoformat()}\n")
        log.write(f"# port={args.port} baud=115200 radar_receive=v1 raw_only=1\n")
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.25)
        connection.dtr = False
        connection.rts = False
        connection.port = args.port
        connection.open()
        with connection:
            deadline = time.monotonic() + args.seconds
            buffer = bytearray()
            while time.monotonic() < deadline:
                buffer.extend(connection.read(connection.in_waiting or 1))
                while b"\n" in buffer:
                    line, _, buffer = buffer.partition(b"\n")
                    text = line.decode("ascii", errors="replace").rstrip("\r")
                    log.write(text + "\n")
                    log.flush()
                    print(text, flush=True)
                    raw = re.fullmatch(r"RAW (\d+) (\d+) (\d+) ([0-9A-F]+)", text)
                    status = re.fullmatch(r"STATUS ms=(\d+) bytes=(\d+) errors=(\d+) pending=(\d+)", text)
                    if raw:
                        seq, stamp, errors = map(int, raw.groups()[:3])
                        if len(raw[4]) % 2:
                            faults.append("odd hex length")
                        else:
                            received += len(bytes.fromhex(raw[4]))
                        if previous_sequence is not None and seq != previous_sequence + 1:
                            faults.append("raw record sequence gap or controller restart")
                        previous_sequence = seq
                        records += 1
                    elif status:
                        stamp, byte_count, errors, pending = map(int, status.groups())
                        statuses += 1
                        if args.expect_idle and (byte_count or pending):
                            faults.append("unexpected input on disconnected receiver")
                    else:
                        # A capture can start partway through a line. Later malformed
                        # records are a transport fault, never valid radar evidence.
                        if records or statuses:
                            faults.append("unexpected/truncated console record or restart")
                        continue
                    if errors:
                        faults.append("UART error/overflow reported")
                    if previous_ms is not None and stamp < previous_ms:
                        faults.append("controller clock restart/wrap")
                    previous_ms = stamp
            if buffer:
                log.write("# trailing partial record: " + repr(bytes(buffer)) + "\n")
    if args.expect_idle:
        if statuses < 3 or received:
            faults.append("idle check requires three heartbeats and no received data")
    elif not records:
        faults.append("no radar bytes captured")
    result = "FAIL" if faults else "PASS"
    scope = "disconnected receiver check" if args.expect_idle else "raw capture only; protocol not validated"
    print(f"{result}: {scope}; records={records} bytes={received} heartbeats={statuses}")
    for fault in sorted(set(faults)):
        print(f"  {fault}")
    return bool(faults)


if __name__ == "__main__":
    raise SystemExit(main())
