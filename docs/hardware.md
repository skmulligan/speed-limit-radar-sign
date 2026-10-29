# Hardware supported by this prototype

| Component | Recorded build |
| --- | --- |
| Controller | ESP32-S3-N16R8, YD-style clone, 2 × 22 headers; 16 MB flash and 8 MB PSRAM identified and exercised |
| Radar | Hi-Link HLK-LD2451, UART; actual firmware version unknown |
| Display | One red P10(1R)-4S-V10 panel, 32 × 16, four-phase HUB12-style scanning; second panel unsupported by this build |
| Level shifter | Purple CJMCU/MCU-245 SN74LVC4245A module, 12 contacts on one side and 10 on the other |
| Assembly | 150 × 100 mm, 2.54 mm isolated-pad protoboard, removable modules |
| Battery | DeWalt DCB205 20V MAX 5 Ah pack, 18 V nominal, matching charger |
| Adapter | TANHEYIN B0FM2XYRKW switched battery adapter, supplied fuse; exact delivered wiring must be checked |
| Cutoff | XH-M609-type module, supplied as MTDELE B0FJ8QK6YL |
| Converter | YIPIN HEXHA B09SD7P92Z fixed nominal 5 V converter; seller claims 10–35 V input, 10 A / 50 W output, not a verified sustained rating |

Product/listing identifiers help identify components, not connector wiring or
safe continuous load. See [primary references](references.md), the
[complete BOM](../hardware/protoboard/bom.md) and
[connection tables](../hardware/protoboard/README.md).

## Compatibility boundaries

ESP32 header-row spacing was confirmed as **25.40 mm**, shifter spacing as
**15.24 mm**, both at 2.54 mm pin pitch. These supersede earlier estimates. Measure
your parts and dry-fit before soldering. GPIO35–37 are reserved for this board's
octal PSRAM; use the recorded GPIO map rather than a different ESP32 pinout.

The panel mapping and high-active ENABLE were exercised on the recorded panel.
Other products sold as P10/HUB12 can differ. DISP carries signals/grounds only;
panel power has its own harness. The radar uses direct 3.3 V UART, bypassing the
5 V display shifter. Check terminal labels, pin 1 and unpowered continuity.

The manufacturer documents nominal 5 V radar supply and 3.3 V I/O. V1.03 asks for
more than 200 mA source capacity; the older V1.00 manual asks for more than 300 mA.
Retain the larger planning allowance. Supply-voltage tolerance is unspecified;
observed operation near 5.17 V does not establish an allowable voltage range.

## Power and installation

Cutoff settings are **18.0 V with 2.0 V hysteresis** (intended reconnect 20.0 V).
Settings persisted through power cycling; actual switching has not been measured.
These are project settings, not manufacturer-certified pack protection or individual
cell monitoring. Remove the battery after use/cutoff; recharge with its matching charger.

Keep separately fused controller and peripheral positive feeds, common ground,
and a separate panel-current return. Final panel-output protection and peripheral
fuse/load compatibility remain open. The adapter's supplied 30 A fuse does not
replace the smaller downstream branch protection described in the wiring guide.

The enclosure/stand, weather protection, mounting angle and final cable strain
relief need validation. Placement drawings reserve zones rather than define a
fabrication template. Keep the radar antenna path clear and recheck behavior after
mounting. [Validation status](validation.md) distinguishes completed observations
from outstanding tests.

## Optional clock for current firmware

The standalone wiring remains the base assembly. Current firmware can use a
DS3231 for battery-backed calendar time; installation is not validated. See
[RTC selection, proposed connections and primary references](tracking-validation.md#rtc-addition).
Legacy does not use this module. Phone-set time works without it during one boot.
