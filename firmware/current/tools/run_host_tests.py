"""Compile and run every firmware host scenario; no hardware is accessed.

Run in the Python environment described in the root README. Requires a C++17 compiler (CXX or c++).
Outputs and full logs are kept in ignored tmp/firmware-host-tests/current/.
"""
import os
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parents[1] / "tests"
OUT = ROOT / "tmp/firmware-host-tests/current"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    scenarios = [
        ("tracking-runtime", "tracking_host/runtime_check.cpp", "tracking_host/runtime_support", ["-pthread"],
         ["isolation", "overflow", "append-failure", "flush-failure", "barrier", "clock", "freshness", "uart-race", "bucket-boundary", "reply-overflow", "stop-pending-storage", "stop-full-queues", "stop-rearm", "time-disconnect", "time-reconnect", "time-command", "stale-command", "thread-handoffs"]),
        ("tracking-panel", "tracking_host/panel_check.cpp", "tracking_host/panel_support", [], [""]),
        ("tracking", "tracking_host/check.cpp", None, [], [""]),
        ("journal", "tracking_host/journal_check.cpp", "tracking_host/support", [], [""]),
        ("calendar", "tracking_host/calendar_check.cpp", None, [], [""]),
        ("radar-query", "radar_query_host/check.cpp", None, [], [""]),
        ("panel", "panel_bench_host/check.cpp", "panel_bench_host", [], [""]),
        ("live", "live_bench_host/check.cpp", "live_bench_host", [], [""]),
        ("warning", "warning_bench_host/check.cpp", "live_bench_host", [], [""]),
    ]
    for daylight in (False, True):
        prefix = "daylight" if daylight else "battery"
        scenarios.append((
            f"{prefix}-panel", "panel_bench_host/battery_check.cpp", "panel_bench_host",
            ["-DDAYLIGHT_PANEL_BENCH"] if daylight else [],
            ["", "cancel", "stop", "rollover"],
        ))
        scenarios.append((
            f"{prefix}-radar", "battery_bench_host/check.cpp", "live_bench_host",
            ["-DDAYLIGHT_RADAR_BENCH"] if daylight else [], ["", "rollover", "stop"],
        ))
    for name, source in (("startup", "startup_check.cpp"), ("continuous", "continuous_check.cpp")):
        scenarios.append((
            name, f"battery_bench_host/{source}", "live_bench_host", [],
            ["normal", "rollover", "stop-pending", "stop-marker", "stop-live"],
        ))
    count = 0
    for name, source, include, flags, modes in scenarios:
        binary = OUT / name
        command = compiler + ["-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic"] + flags
        if include:
            command += ["-I", str(TESTS / include)]
        command += [str(TESTS / source), "-o", str(binary)]
        with (OUT / f"{name}.log").open("w") as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            for mode in modes:
                subprocess.run([str(binary)] + ([mode] if mode else []), cwd=ROOT,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
                count += 1
        print(f"PASS {name}: {len(modes)} scenario(s)", flush=True)
    print(f"PASS all {count} scenarios across {len(scenarios)} host builds; logs: {OUT}")


if __name__ == "__main__":
    main()
