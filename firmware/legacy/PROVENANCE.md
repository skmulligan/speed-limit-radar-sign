# Frozen legacy firmware provenance

Source revision: `23136dac90a0aea43c185084bb4d1172e7f83c96` (last pre-logging revision).
Normal sign target: `continuous_radar`.

`SHA256SUMS` records SHA-256 digests of every original runtime source/header,
PlatformIO configuration, test source/adapter, and serial helper. These files are
byte-for-byte copies of that revision. No source or dependency pin was changed.
Verify from this directory with `shasum -a 256 -c SHA256SUMS`.

Packaging-only changes: this README and per-suite documentation use the new
paths (including isolated manual-test outputs); `tools/run_host_tests.py` locates this edition's tests and the repository
root, and writes to `tmp/firmware-host-tests/legacy/`. Its scenarios are unchanged. Public setup wording refers to the root Python
environment rather than the maintainer’s Conda environment.
The original historical README remains available in the source revision; the
builder guide replaces it here. Curated captured fixtures remain at repository
level with their provenance. They are not firmware dependencies or shared code.

Legacy is frozen. New features and firmware fixes belong in `../current/`.
Any future exception requires an explicit decision and updated provenance.
Local customization is possible, but modified files no longer match this snapshot.
The public companion includes this snapshot directly and does not require access
to the source repository's history.
