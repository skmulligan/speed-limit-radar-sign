"""Keep COM open between panel bench commands; opening COM may reset this board.

Run in cryoem with the panel OFF initially. This tool never commands a test
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
    commands = {'status', 'stop', 'test', 'map', 'digits', 'probe-low', 'probe-high', 'oe-off-low', 'oe-off-high',
                'blank-bit-0', 'blank-bit-1'}
    identified = False
    pending = bytearray()
    with args.output.open('xb') as log, port:
        print('CONSOLE OPEN: keep panel power OFF until PANEL_BENCH is identified.', flush=True)
        print('Commands: status, stop, test, map, digits, probe-low, probe-high; configuration only with panel OFF; quit after power OFF.', flush=True)
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
                            if text.startswith(('PANEL_BENCH v1 ', 'PANEL_BENCH v2 ', 'PANEL_BENCH v3 ', 'PANEL_BENCH v4 ', 'PANEL_BENCH trial=1 ')):
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
                        print('REJECTED: PANEL_BENCH firmware has not been identified', flush=True)
                    else:
                        log.write(f'HOST {time.monotonic():.3f} {command}\n'.encode())
                        log.flush()
                        port.write((command + '\n').encode())
                        port.flush()
        finally:
            # Best effort only: stop returns to the unverified blanking trial.
            # Physical power removal is still required before closing/resetting.
            if identified and port.is_open:
                port.write(b'stop\n')
                port.flush()


if __name__ == '__main__':
    raise SystemExit(main())
