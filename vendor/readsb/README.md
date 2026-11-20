# Bundled readsb (ADSB/Mode-S decoder)

`readsb` is built from <https://github.com/wiedehopf/readsb> with the RTL-SDR
backend enabled and installed as `/usr/lib/zdr/readsb`; zDR prefers it
over a system `/usr/bin/readsb`.

- Upstream commit: `3f064e2` (full hash and SHA-256 in [`SOURCE.md`](SOURCE.md))
- Rebuild: `./scripts/build-readsb.sh`
- License: GPL-3.0-or-later (see the upstream `COPYING`)

The Debian package `readsb` in trixie is built **without** the RTL-SDR backend
(`Support for this SDR type was not enabled in this build.`), which is why this
binary is bundled instead of being declared as a dependency.
