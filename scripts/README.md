# Scripts

## `build-readsb.sh`

Rebuild the pinned [wiedehopf/readsb](https://github.com/wiedehopf/readsb)
binary (RTL-SDR backend) into `vendor/readsb/` and refresh `SOURCE.md`.

```bash
# On an aarch64 machine or arm64 container (Cardputer / Docker):
sudo apt-get install -y build-essential git librtlsdr-dev
./scripts/build-readsb.sh

# Optional: override the pin
READSB_COMMIT=<full-sha> ./scripts/build-readsb.sh
```

Commit the updated `vendor/readsb/readsb`, `SOURCE.md`, and `README.md` when
the hash changes.

## CI `.deb`

Push to `develop`, push a `v*` tag, or run **Package deb** via Actions → workflow_dispatch.
(Plain pushes to `main` do not package.) Download the `zdr-arm64-deb` artifact and on the Cardputer:

```bash
scp zdr_*.deb pi@<cardputer-ip>:/tmp/
ssh pi@<cardputer-ip> 'sudo apt install -y /tmp/zdr_*.deb'
```
