"""Persistent attended radar query console; no commands sent automatically.

Use cryoem. Open with radar USB and battery off. Power radar before 'query'.
Keep COM open through completion; do not reset/unplug MCU during configuration.
"""
import argparse
import os
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
    identified = active = False
    pending = bytearray()
    host = bytearray()
    stdin_open = True
    with args.output.open('xb') as log, port:
        print('CONSOLE OPEN: query/status/stop; no automatic query. Keep radar off until startup verified.', flush=True)
        while True:
            watched = [port.fileno()] + ([sys.stdin.fileno()] if stdin_open else [])
            ready, _, _ = select.select(watched, [], [], 0.1)
            if port.fileno() in ready:
                data = port.read(port.in_waiting or 1)
                log.write(data)
                log.flush()
                pending.extend(data)
                while b'\n' in pending:
                    line, _, pending = pending.partition(b'\n')
                    text = line.decode('ascii', errors='replace').rstrip('\r')
                    print(text, flush=True)
                    if text.startswith('RADAR_QUERY v1 '):
                        identified = True
                    if text == 'QUERY_STARTED':
                        active = True
                    if text in ('QUERY_COMPLETE', 'QUERY_FAILED'):
                        active = False
                if len(pending) > 4096:
                    raise RuntimeError('Oversized serial line')
            if sys.stdin.fileno() in ready:
                data = os.read(sys.stdin.fileno(), 4096)
                if not data:
                    stdin_open = False
                    print('Input closed; waiting for any active query cleanup.', flush=True)
                host.extend(data)
                while b'\n' in host:
                    raw, _, host = host.partition(b'\n')
                    command = raw.decode('ascii', errors='replace').strip()
                    if command == 'quit':
                        if active:
                            print('REJECTED: wait for query cleanup before closing COM.', flush=True)
                        else:
                            return
                    elif command not in {'query', 'status', 'stop'} or not identified:
                        print('REJECTED: unknown command or firmware not identified.', flush=True)
                    elif command == 'query' and active:
                        print('REJECTED: query already running.', flush=True)
                    else:
                        if command == 'query':
                            active = True
                        log.write(f'HOST {time.monotonic():.3f} {command}\n'.encode()); log.flush()
                        port.write((command+'\n').encode()); port.flush()
                if len(host) > 4096:
                    host.clear()
                    print('REJECTED: oversized host command.', flush=True)
            if not stdin_open and not active:
                return


if __name__ == '__main__':
    main()
