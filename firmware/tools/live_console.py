"""Keep COM open between live bench commands; opening COM may reset this board.

Run with the panel OFF initially. This tool never commands a test
automatically. Disconnect panel power before quitting/reopening the console.
"""

import argparse
from pathlib import Path
import select
import sys
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(port=None, baudrate=115200, timeout=0)
    port.dtr = False
    port.rts = False
    port.port = args.port
    commands = {'status', 'stop', 'live'}
    identified = False
    pending = bytearray()
    with args.output.open('xb') as log, port:
        print('CONSOLE OPEN: keep panel power OFF until LIVE_BENCH is identified.', flush=True)
        print('Commands: status, stop, live (20 seconds); quit only after battery power OFF.', flush=True)
        try:
            while True:
                ready, _, _ = select.select([port.fileno(), sys.stdin], [], [], 0.1)
                if port.fileno() in ready:
                    data = port.read(port.in_waiting or 1)
                    if data:
                        log.write(data)
                        log.flush()
                        pending.extend(data)
                        while b'\n' in pending:
                            line, _, pending = pending.partition(b'\n')
                            text = line.decode('utf-8', errors='replace').rstrip('\r')
                            print(text, flush=True)
                            if text.startswith(('LIVE_BENCH v1 ', 'LIVE_STATUS ')):
                                identified = True
                        if len(pending) > 4096:
                            raise RuntimeError('Unexpected oversized serial line')
                if sys.stdin in ready:
                    line = sys.stdin.readline()
                    if not line:
                        return 0
                    command = line.strip()
                    if command == 'quit':
                        return 0
                    if command not in commands:
                        print('REJECTED: unknown command', flush=True)
                    elif not identified:
                        print('REJECTED: LIVE_BENCH firmware has not been identified', flush=True)
                    else:
                        log.write(f'HOST {time.monotonic():.3f} {command}\n'.encode())
                        log.flush()
                        port.write((command + '\n').encode())
                        port.flush()
        finally:
            # Best effort only: stop disables panel enable and clears the current sample.
            # Physical power removal is still required before closing/resetting.
            if identified and port.is_open:
                port.write(b'stop\n')
                port.flush()


if __name__ == '__main__':
    raise SystemExit(main())
