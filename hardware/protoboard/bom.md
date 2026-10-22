# Complete build BOM

Parts for the recorded one-panel prototype. Selection guidance is not a universal
rating or proof of fit. See [hardware details](../../docs/hardware.md) and
[validation](../../docs/validation.md) for evidence and outstanding checks.

## Main modules

| Qty | Item | Identification / requirement |
| ---: | --- | --- |
| 1 | ESP32-S3-N16R8 YD-style board | 16 MB flash, 8 MB octal PSRAM, 2 × 22 headers; 25.40 mm row centres |
| 1 | HLK-LD2451 radar | Nominal 5 V, 3.3 V UART; allow >300 mA source capacity per older manual |
| 1 | Red P10(1R)-4S-V10 panel | 32 × 16, four-phase scan; exact recorded cable mapping only |
| 1 | CJMCU/MCU-245 SN74LVC4245A module | Purple 12+10-contact version; 15.24 mm row centres |
| 1 | DeWalt DCB205 battery and compatible charger | 20V MAX / 18 V nominal, 5 Ah baseline; runtime unverified |
| 1 | Switched, fused battery adapter | Recorded TANHEYIN B0FM2XYRKW; verify routing/polarity |
| 1 | XH-M609-type low-voltage cutoff | Recorded MTDELE B0FJ8QK6YL; actual switching remains unverified |
| 1 | Nominal 5 V buck converter | Recorded YIPIN HEXHA B09SD7P92Z; seller's 10 A rating is not verified continuous output |
| 1 each | Panel signal cable and separate power harness | Check every JIN conductor; panel return goes directly to distribution |
| As needed | Insulated backing, enclosure, stand and fasteners | Final mechanical/weather design and panel-output protection remain open |
| 1 | Data-capable COM USB cable | Isolated programming only |
| 1 | Multimeter | Continuity, polarity and staged rail checks |

## Protoboard and wiring

| Qty | Item | Selection / fit check |
| ---: | --- | --- |
| 1 | 150 × 100 mm FR4 isolated-pad protoboard | 2.54 mm pitch; usable area, hole diameter and terminal tails must fit |
| 2 | 1×22 female socket strips | ESP32; 2.54 mm pitch, 25.40 mm row centres; check tails and underside clearance |
| 1 each | 1×12 and 1×10 female socket strips | Shifter; shorter row staggered one pitch at each end |
| 1 | Keyed 2×8 male IDC header | DISP; match existing cable, pin 1, key and body clearance |
| 1 | Keyed 2×5 male IDC header | RAD; 2.54 mm pitch |
| 1 | Mating 2×5 female IDC plug and strain relief | Custom radar harness, approximately 20–30 cm initially |
| 1 | Suitable 1×6 female housing and contacts | Radar's 2.54 mm header; populate VIN/GND/TX/RX positions only |
| 1 length | 10-conductor ribbon and insulated splice materials | Verify conductor/contact suitability for the protected radar branch |
| 2 | 2-position terminal blocks | PWR-E/PWR-P; 5.08 mm pitch, ≥3 A rating, tails/body must fit board and wire |
| 1 | 1×2 male header and removable shunt | ESP-LINK; rated for the ESP power branch |
| 2 | 100 nF ceramic capacitors | C1/C2; ≥25 V; short leads beside shifter supplies |
| 1 | 10 µF electrolytic capacitor | C3; ≥16 V; mark polarity |
| 2 | 10 kΩ axial resistors | R1/R2; ≥1/8 W |
| 1 new | 1 A DC fuse and matching enclosed inline holder | Peripheral feed; ≥32 V DC, provisional pending wiring/startup checks |
| Reuse | Existing 1 A ESP output fuse and holder | Reported fitted; exact installed model not recorded |
| Reuse | 5 A battery-side branch fuse and holder | Before cutoff/converter, separate from supplied adapter fuse |
| As needed | Insulated distribution terminals | Match actual wire sizes, DC ratings and load; panel branch stays off-board |
| As needed | 22 AWG copper power wire; 24–26 AWG signal wire | Short logic wiring, compatible terminations; not panel-feed sizing |
| At least 4 | Board standoffs and suitable fasteners | Actual hole locations/clearances determined by dry fit |
| 1 | Insulating underside cover | Keep clear of solder joints; preserve cooling |
| As needed | Strain relief, heat shrink, labels and module retention | No exposed power tails or sockets supported only by wires |
| If needed | Insulating supports for independent socket strips | Fallback only if actual module rows do not fit the grid |

The main modules above complement this board-level list. Cutoff/converter mount directly to the service plate.
Panel-output protection, final power-harness ratings and enclosure fasteners
remain dependent on actual load and mechanical measurements.

See [the assembly guide](README.md) for wiring, primary references and checks.
