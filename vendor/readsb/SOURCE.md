# Vendored readsb

Built for zDR with the RTL-SDR backend enabled. Rebuild with
`./scripts/build-readsb.sh` (preferably on aarch64 / in an arm64 container).

| Field | Value |
|-------|-------|
| Upstream | https://github.com/wiedehopf/readsb.git |
| Commit | `3f064e2a3a9fb13321088e56f11ce0f68234e15a` |
| Build | `make -j RTLSDR=yes` |
| Binary SHA-256 | `f5ae70efff50aab7cdaca83427627d47220e0d488358aa09e9889aed0cf0e705` |
| `file` | ELF 64-bit LSB pie executable, ARM aarch64 |
| License | GPL-3.0-or-later (upstream `COPYING`) |

Runtime shared libs (for packaging / `ldd`): `librtlsdr`, `libncurses`,
`libtinfo`, `libzstd`, `libz`, `libm`, `libc`.

Debian's `readsb` package often lacks the RTL-SDR backend; this binary is
installed to `/usr/lib/zdr/readsb` and preferred by the ADSB page.
