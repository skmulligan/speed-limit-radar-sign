# Maintaining the public companion

This repository contains the reviewed build companion, with an independent history.
Keep original photos, full bench logs, flash backups, vendor downloads and local
credentials outside the published file tree. Generated outputs belong in the ignored
`output/`, `tmp/`, `logs/` and `.pio/` locations; do not add broad ignore rules for
`.bin` or `.log` because the curated test fixtures use them.

Review future changes as explicit file diffs. Never merge private development history
or use a mirror/all-branches push from a private working repository. Review commit
author/committer identity as well as file contents before publishing. Use a public
handle and GitHub noreply address, and keep signing identity private if applicable.

Firmware changes require host regression tests and builds of affected environments.
Keep reader-facing hardware notes current, but distinguish reported bench results,
software checks, specifications and assumptions. Preserve fixture provenance and label
synthetic data. Do not claim speed calibration or endurance from software simulation.

The README identifies the intended firmware environment. Instructable links and release
binaries can be added when available; do not publish a placeholder URL or an unverified
binary. Hardware designs and photos require a separate inclusion/privacy review.

## Two independent firmware editions

`firmware/legacy/` is the frozen standalone snapshot; verify SHA256SUMS and read
PROVENANCE.md before touching it. `firmware/current/` is the only firmware feature
development destination. Both include their own source, headers, tests, tools
and pinned PlatformIO configuration; neither imports the other. Root fixtures
remain curated inputs with provenance. Host outputs are separated under
`tmp/firmware-host-tests/legacy/` and `tmp/firmware-host-tests/current/`.

`ios/` contains the current firmware companion, shared Swift package and tracked
Xcode project. Keep signing credentials, user state, runtime SQLite databases
and exports local. The optional project generator needs only Python; no private
history or development-only baseline helper is required by any public build.

Use the root firmware selector and edition guides for current commands. Diagnostic
defaults remain intentional; normal targets must be selected explicitly.
