# Releasing Facet

## Channels

- **Dev**: workbench builds from `main`, flashed to test panels, never published.
- **Beta**: a tag `vX.Y.Z-beta.N`. Panels on the beta channel (Settings > Update channel) are offered it.
- **Stable**: a tag `vX.Y.Z`. Its `esphome/facet-*.yaml` files must already say that version.

Pushing a tag starts `.github/workflows/release.yml`. It waits for CI and Firmware to be green on the tagged commit,
builds both boards, checks each image says the version (never `+wip`), and publishes:

- the tag's release with its images (a pre-release for a beta);
- the channel's asset (`firmware` or `firmware-beta`), and moves that channel tag to the build;
- a docs deploy, which serves `manifest.json` + `ota.json` (stable) and `manifest-beta.json` + `ota-beta.json` (beta)
  per board. The Install page offers Stable / Beta while a beta exists.

## Quality checklist (a person, on glass, before a stable tag)

Run on a fresh Waveshare and a fresh Guition. Every line is a yes or the release waits. The workbench shows the same
list (`workbench/src/lib/checklist.ts`).

- Install from the docs site in Chrome on a panel that has never run Facet. First boot reaches set up without help.
- Set up: join Wi-Fi, link Home Assistant, land on Home with the factory deck.
- Open the panel's address in a browser: the editor loads, reorder the deck, change one screen's entity, see it on
  the glass within a few seconds. Revert works.
- Every shipped screen with real data, and with its entity missing or unavailable: empty and loading states, no crash,
  no stale paint.
- Dark, light, auto; two accents; both panels; text never clipped at either size.
- Pull the network: screens show offline states; restore it: they recover without a reboot.
- An update arrives in Home Assistant, installs, the probation chip shows and clears; pull power during probation and
  the previous image boots.
- Both panels run 24 hours on the release candidate: no reboot, heap stable (`/fw/info`).
- Tap, swipe, long press on every screen: one sound per tap, nothing fires when disabled.
- Read the README, the Install page and the Contributing page as a stranger: every link resolves, every screenshot
  matches the build.

## Release day

V = the version, for example `1.0.0`. A failed check stops the day.

1. `git switch main && git pull && git status --short`: empty, and the last nightly gate green.
2. `(cd renderer && node tools/gen_catalogue.mjs) && git status --short`: empty (else commit "Catalogue: V").
3. Read the root README: the screens table is the shipped set (`renderer/features.json` `firmware: true`); the panels
   table says exactly what is missing, if anything.
4. `node firmware/release-prep.mjs V`: sets `facet_version` in both `esphome/facet-*.yaml`, publishes the Updates
   entry dated today. `git diff --stat` shows those files only.
5. `(cd site && FACET_DOCS_BASE=/facet/ FACET_DOCS_OUT=dist.release npm run build)`: `check-dist: N pages, 0 faults`.
6. Commit "Release: V", `git tag -a vV -m "Facet V"`, `git push origin main vV`.
7. Watch the release workflow (`gh run list --workflow release.yml`); then
   `https://mrdarrengriffin.github.io/facet/firmware/<board>/manifest.json` says V.
8. Install from the docs site on a fresh panel (the checklist above); the 24 h run on this build starts.

To build the images locally instead (no publishing): `esphome/release.sh V [out dir]` builds from a worktree of the tag.

## Task references

Code comments and commit messages that say `facet-NNN` refer to an item on the public project board,
<https://github.com/users/mrdarrengriffin/projects/3>.
