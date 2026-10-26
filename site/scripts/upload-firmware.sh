#!/bin/bash
# Publish the public firmware for the docs /install page and the panels' update check, then redeploy.
#   site/scripts/upload-firmware.sh [dir]      (needs gh, signed in; dir default site/public/firmware)
# dir/<board>/ holds manifest.json (ESP Web Tools: factory image) + ota.json (ESPHome update entity: OTA image) and
# the images they name. Only the files the two manifests reference go into the `firmware` release asset
# (firmware.tar.gz); .github/workflows/deploy.yml unpacks it into site/public/firmware/ before the Astro build, so they
# are served at /facet/firmware/<board>/... One copy per board: each upload replaces the last.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
D=$(cd "${1:-$ROOT/site/public/firmware}" && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
files=$(python3 - "$D" <<'PY'
import json, os, sys
d = sys.argv[1]
out = []
for board in sorted(os.listdir(d)):
    bd = os.path.join(d, board)
    if not os.path.isfile(os.path.join(bd, "manifest.json")): continue
    man = json.load(open(os.path.join(bd, "manifest.json")))
    ota = json.load(open(os.path.join(bd, "ota.json")))
    if man["version"] != ota["version"]: sys.exit(f"{board}: manifest {man['version']} != ota {ota['version']}")
    paths = [p["path"] for b in man["builds"] for p in b["parts"]] + [b["ota"]["path"] for b in ota["builds"]]
    for p in paths:
        if not os.path.isfile(os.path.join(bd, p)): sys.exit(f"{board}: missing {p}")
    out += [f"{board}/manifest.json", f"{board}/ota.json"] + [f"{board}/{p}" for p in paths]
    print(f"{board} {man['version']}", file=sys.stderr)
if not out: sys.exit(f"no <board>/manifest.json in {d}")
print("\n".join(out))
PY
)
tar -czf "$T/firmware.tar.gz" -C "$D" $files
cd "$ROOT"
gh release view firmware >/dev/null 2>&1 || gh release create firmware --prerelease --title "Firmware (install page)" \
  --notes "Public firmware images + ESP Web Tools / OTA manifests per board, served by the docs site at /facet/firmware/<board>/ (Pages workflow). Replaced on every firmware publish; not a release."
gh release upload firmware "$T/firmware.tar.gz" --clobber
gh workflow run deploy.yml --ref main
