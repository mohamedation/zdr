#!/usr/bin/env bash
# Build a pinned wiedehopf/readsb with the RTL-SDR backend and install it
# into vendor/readsb/ for the zDR package.
#
# Usage:
#   ./scripts/build-readsb.sh              # use the pinned commit below
#   READSB_COMMIT=<sha> ./scripts/build-readsb.sh
#
# Requires: git, make, a C toolchain, librtlsdr development headers.
# On Cardputer / aarch64 the resulting binary can be committed as-is.
# Cross-building: run this script inside an aarch64 chroot/container.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT/vendor/readsb"
# readsb 3.16.21-era pin (same as historical zeroSDR vendor notes).
PINNED_COMMIT="3f064e2a3a9fb13321088e56f11ce0f68234e15a"
COMMIT="${READSB_COMMIT:-$PINNED_COMMIT}"
SRC_URL="https://github.com/wiedehopf/readsb.git"
BUILD_DIR="${TMPDIR:-/tmp}/zdr-readsb-build-$$"

cleanup() { rm -rf "$BUILD_DIR"; }
trap cleanup EXIT

echo "Building readsb @ ${COMMIT}"
mkdir -p "$BUILD_DIR"
git clone --filter=blob:none --no-checkout "$SRC_URL" "$BUILD_DIR/readsb"
git -C "$BUILD_DIR/readsb" fetch --depth 1 origin "$COMMIT"
git -C "$BUILD_DIR/readsb" checkout --detach "$COMMIT"

RESOLVED="$(git -C "$BUILD_DIR/readsb" rev-parse HEAD)"
if [[ "$RESOLVED" != "$COMMIT"* && "$COMMIT" != "$RESOLVED"* ]]; then
    echo "error: checked out $RESOLVED, expected $COMMIT" >&2
    exit 1
fi

make -C "$BUILD_DIR/readsb" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" RTLSDR=yes

mkdir -p "$OUT_DIR"
install -m 755 "$BUILD_DIR/readsb/readsb" "$OUT_DIR/readsb"

if command -v sha256sum >/dev/null 2>&1; then
    HASH="$(sha256sum "$OUT_DIR/readsb" | awk '{print $1}')"
elif command -v shasum >/dev/null 2>&1; then
    HASH="$(shasum -a 256 "$OUT_DIR/readsb" | awk '{print $1}')"
else
    HASH="(install coreutils for sha256)"
fi

ARCH="$(file -b "$OUT_DIR/readsb" 2>/dev/null || echo unknown)"

cat > "$OUT_DIR/SOURCE.md" <<EOF
# Vendored readsb

Built by \`scripts/build-readsb.sh\` for zDR (RTL-SDR backend enabled).

| Field | Value |
|-------|-------|
| Upstream | ${SRC_URL} |
| Commit | \`${RESOLVED}\` |
| Build | \`make -j RTLSDR=yes\` |
| Binary SHA-256 | \`${HASH}\` |
| \`file\` | ${ARCH} |
| License | GPL-3.0-or-later (upstream \`COPYING\`) |

Debian's \`readsb\` package often lacks the RTL-SDR backend; this binary is
installed to \`/usr/lib/zdr/readsb\` and preferred by the ADSB page.
EOF

# Keep a short README pointer in sync.
cat > "$OUT_DIR/README.md" <<EOF
# Bundled readsb (ADSB/Mode-S decoder)

\`readsb\` is built from <${SRC_URL}> with the RTL-SDR backend enabled and
installed as \`/usr/lib/zdr/readsb\`; zDR prefers it over a system
\`/usr/bin/readsb\`.

- Upstream commit: \`${RESOLVED:0:7}\` (full: see \`SOURCE.md\`)
- Rebuild: \`./scripts/build-readsb.sh\`
- License: GPL-3.0-or-later (see the upstream \`COPYING\`)

The Debian package \`readsb\` in trixie is built **without** the RTL-SDR backend
(\`Support for this SDR type was not enabled in this build.\`), which is why this
binary is bundled instead of being declared as a dependency.
EOF

echo "Installed $OUT_DIR/readsb"
echo "SHA-256: $HASH"
echo "Recorded $OUT_DIR/SOURCE.md"
