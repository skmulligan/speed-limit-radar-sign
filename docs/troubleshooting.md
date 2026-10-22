# Troubleshooting

Disconnect every power source before rewiring. Use the [wiring tables](../hardware/protoboard/README.md)
and match the actual board labels. Do not reconnect COM USB to the assembled
battery-powered sign merely to obtain logs; use the documented isolated service setup.

| Symptom | Checks |
| --- | --- |
| Build succeeds but no STARTING | Confirm `-e continuous_radar`; default `diagnostic` intentionally drives no panel. Check panel supply, JIN connection and the exact mapped signals. |
| STARTING then blank | Expected without a fresh eligible target. Check radar power, common ground and TX → GPIO17; a startup label does not establish radar reception. |
| Speeds vanish quickly | No-target/invalid reports clear immediately; silence expires at 750 ms. Review mounting, coverage and raw data before changing freshness. |
| Wrong pixels or inversion | Compare the exact panel revision, connector orientation, cable continuity and shifter wiring; generic P10 diagrams may differ. Use bounded diagnostics only after wiring checks. |
| UART faults/backlogs | Check supply stability, ground, RX wiring and single-source UART connections. Continuous firmware deliberately clears data on errors. |
| Resets or flicker | Check loaded rails, connectors, panel-current return and supply capacity. Repeat short attended checks before longer runs; do not mask faults in software. |
| Upload cannot connect | Use COM USB, correct detected port and full external isolation. Close other serial tools; keep configured upload speed 115200. |
| Query ends QUERY_FAILED | Inspect per-command ACK/status. Version read timed out on the tested unit even when detection/sensitivity readback succeeded. |
| Console fails on Windows | Persistent console scripts use POSIX file-descriptor selection. Use PlatformIO monitor for manual commands. |

Serial opening may reset the controller. Automatic builds restart their timed or
continuous behavior at reset. A synthetic demo is a display check, not a speed test.
Record actual observations separately from guesses about their cause.
