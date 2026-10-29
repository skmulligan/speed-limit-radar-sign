# Street Speed — iPhone app

For **current firmware only** (`firmware/current`, environment `tracking`);
legacy has no app connection.

Native SwiftUI app for iOS 17+, with a local SQLite history database and a custom
Core Bluetooth connection to the ESP32 `tracking` firmware. First delivery is a
personal Xcode install. No account, internet connection, backend or paid library
is used. Signing credentials are intentionally not committed.

Open `StreetSpeed.xcodeproj`, select the **StreetSpeed** scheme, choose your
personal development team in Signing & Capabilities, select your iPhone and run.
Change the bundle identifier if your team requires a unique value. Xcode may
require Developer Mode/trust on the phone. Real BLE and pairing must be tested
on an iPhone; simulator checks do not validate the accessory connection.

Screens:

- **Radar:** current MPH, warning, sensor-relative range/angle dots and all fresh
  target details, with the sign-selected report entry highlighted. Angular
  orientation and direction labels remain provisional. The forward semicircle
  uses 25 m rings and soft side shading around the manual's nominal ±20° cone;
  this is a visual guide, not measured coverage or an additional target filter.
  Invalid/stale readings
  disappear; plot dots have no persistent vehicle identity.
- **History:** offline episode list with speeding, day and session filters;
  episode detail with one-second min/mean/max speed vector; CSV sharing.
- **Trends:** hourly detection-episode totals and peak speeds. These are not
  vehicle counts. Unknown calendar dates are excluded.
- **Device:** discover/connect, explicit Sync now, last sync, storage, gaps,
  error counts, clock health and Set clock from this iPhone.

## First connection

1. Follow the firmware/RTC commissioning steps in
   [tracking-validation.md](../docs/tracking-validation.md). Tracking firmware was
   installed on 2026-10-27 and passed isolated USB startup/storage checks. The
   owner subsequently confirmed first phone pairing, and serial confirmed
   phone-set time. The owner also confirmed live walking detections/clearing and
   an event in History after sync, persistence after app reopening, disconnected
   logging/download and duplicate-free repeat sync. The subsequent sign
   power-cycle tests were reported qualitatively successful; detailed results
   are in the commissioning notes. These observations apply to earlier tracking firmware; v2 has only isolated
   startup/storage checks recorded. RTC and extended field tests remain pending.
2. With the board in the established isolated USB-console setup, issue `pair`.
   In the app's Device screen, choose Find nearby sign and select Street Speed.
   Enter the six-digit passkey printed in the console when iOS requests it.
3. Choose **Set clock from this iPhone**. Without an initialized/valid RTC,
   calendar dates are unknown after complete power removal; monotonic logging
   still works. The app labels any same-session time reconstruction.
4. After restoring the verified battery assembly with power removed, reconnect
   from the phone. **Sync now** imports committed records and resumes from its
   durable cursor. History remains usable while disconnected.

Bluetooth permission denial is handled in the app; re-enable it in iOS Settings.
Only one owner phone is bonded. To replace it, use `forget-phone` on the USB
console, remove the old iOS bond if necessary, then repeat `pair`. Unsynced
records are never automatically reclaimed. Do not delete the app as a backup
strategy: the device may already have reclaimed acknowledged history. Export
CSV if you need another copy.

The first release works in the foreground. Suspension clears the current view;
returning reacquires a clock mapping and fresh data. A stalled sync can be
restarted with Sync now. Large-backlog performance has not been measured.

## Development and checks

The app icon is the owner-selected dimensional radar snail, derived from
original project snail artwork using built-in image generation. The source
artwork is not included in this companion; the app icon is included. Its opaque 1024 × 1024
PNG lives in `SpeedDetector/Assets.xcassets/AppIcon.appiconset`. Xcode generates
the device sizes using Apple's [single-image asset catalog workflow](https://developer.apple.com/documentation/xcode/configuring-your-app-icon).
The project generator includes the catalog and selects `AppIcon` in both build
configurations. Rebuild and run from Xcode to update the icon on an installed app.

The project is generated deterministically by `generate_project.py` using Python
3.10+. Use the virtual environment from the root guide. Track the generated project and shared scheme; local signing
changes and Xcode user data remain local. No external Swift package downloads
are needed.

```sh
python ios/generate_project.py
swift test --package-path ios/SpeedDetectorCore --scratch-path tmp/swift-build
xcodebuild -project ios/StreetSpeed.xcodeproj -scheme StreetSpeed \
  -destination 'generic/platform=iOS Simulator' -derivedDataPath tmp/xcode-build \
  CODE_SIGNING_ALLOWED=NO build
```

Run the StreetSpeed UI test against an available iPhone simulator in Xcode.
Its `--ui-testing` launch mode disables Bluetooth and uses an empty in-memory
history database. This mode never creates synthetic real-world detections.
The C++-generated binary under the Swift package's test Fixtures directory is
explicitly synthetic; its provenance and regeneration command are alongside it.

The package tests cover record/fragment integrity, conservative freshness,
transaction rollback, duplicate/conflicting records, persistent reopen and
explicit retention gaps. The protocol is documented in
[tracking-protocol.md](../docs/tracking-protocol.md).

Apple Watch is deferred. The shared package provides reusable models and
freshness rules; a future iPhone-relayed Watch app can show radar and new events
without duplicating the parser or treating delayed history as live data.
