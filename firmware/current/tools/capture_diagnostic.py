"""Capture the diagnostic's serial response and require a successful repeat test.

Run with the cryoem Conda environment. Only use after flashing diagnostic-v1.
This sends 'r' once after observing its heartbeat; it never writes flash.
"""

import argparse
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    device = serial.Serial(port=None, baudrate=115200, timeout=0.5)
    device.dtr = False
    device.rts = False
    device.port = args.port
    requested = False
    began = False
    passed = False
    healthy_heartbeats = 0
    patterns = set()
    started = time.monotonic()
    with args.output.open("wb") as log, device:
        while time.monotonic() - started < args.timeout:
            raw = device.readline()
            if not raw:
                continue
            log.write(raw)
            log.flush()
            line = raw.decode("utf-8", errors="replace").strip()
            print(line, flush=True)
            if requested and line.startswith("DIAG BEGIN"):
                began = True
            if began and ("DIAG FAIL" in line or "TEST FAIL" in line or "Guru Meditation" in line):
                return 1
            if began and line.startswith("TEST PASS pattern="):
                patterns.add(line.split("pattern=", 1)[1].split()[0])
            if began and line.startswith("DIAG PASS"):
                passed = "tested_bytes=7340032" in line and "heap_ok=1" in line
            if passed and line.startswith("HEARTBEAT"):
                if "status=PASS" not in line:
                    return 1
                healthy_heartbeats += 1
                if healthy_heartbeats >= 5 and patterns == set("012345"):
                    print("CAPTURE PASS: repeat command, six patterns, and five healthy heartbeats")
                    return 0
            if not requested and line.startswith("HEARTBEAT") and "free_psram=" in line:
                device.write(b"r")
                device.flush()
                requested = True
        print("CAPTURE FAIL: timed out before a complete repeat test and heartbeat check")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
