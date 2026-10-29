# Third-party notices

The root MIT license covers original project source and accompanying original
documentation. It does not relicense external dependencies or referenced works.

## Build and tooling dependencies

PlatformIO Core (Apache-2.0), Arduino-ESP32 (LGPL-2.1-or-later), its ESP-IDF components
(Apache-2.0 and component-specific licenses), pyserial (BSD) and optional Matplotlib
(its own license) are installed separately. Their distributions retain authoritative
copyright/license notices, including licenses of bundled components and tools.
No dependency source or compiled firmware is vendored in this repository.
If distributing binaries later, review the corresponding dependency obligations.

## Display reference

Freetronics DMD by Marc Alexander was consulted for four-phase scan order and signal
polarity. Its [DMD.cpp](https://github.com/freetronics/DMD/blob/master/DMD.cpp) and
[DMD.h](https://github.com/freetronics/DMD/blob/master/DMD.h) identify GPL version 3.
Those files, fonts and library are not included or linked. The project uses its own
coordinate mapping and GPIO refresh implementation. Source review for this package
found no imported DMD implementation; this reference is not being offered under MIT.

## Hardware and protocol references

Hi-Link, Texas Instruments, Espressif, DeWalt and other reference authors retain rights
to their documents. This repository links to reference material rather than redistributing
vendor archives, CAD or manuals. Project wiring drawings describe this prototype and
are not vendor-approved designs. Captured fixtures record device outputs; their companion
notes document provenance and transformations without distributing complete bench logs.

## iOS companion

The app uses Apple SwiftUI/Core Bluetooth and the system SQLite library through
the platform SDK; no external Swift packages or SQLite implementation are vendored.
Their platform distributions retain applicable terms. The original app source
and included project icon are covered by this repository’s MIT license.
