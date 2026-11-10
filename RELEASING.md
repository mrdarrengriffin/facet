# Releasing Facet

## Channels

A panel's Update channel (Settings, and the Update channel select in Home Assistant) picks which manifest its Firmware
update entity checks. The default is Stable; a fresh panel starts on the channel of the build it was flashed with.

- **Stable**: a tag `vX.Y.Z`. Its `esphome/facet-*.yaml` files must already say that version. `ota.json`, and the
  Install page's default (`manifest.json`).
- **Beta**: a tag `vX.Y.Z-beta.N`. `ota-beta.json`, and the Install page's Beta switch (`manifest-beta.json`).
- **Dev**: a nightly build of `main` by `.github/workflows/dev.yml`, untested beyond CI. `ota-dev.json` only: the
  Install page never offers it. It builds once a night when `main` moved since the last dev build (the `firmware-dev`
  tag) and CI and Firmware are green on it; `gh workflow run dev.yml` builds one now. Its version is the last tag's
  plus the commit's time (UTC, to the minute) and the commit: after `v0.5.0-beta.5`,
  `0.5.0-beta5-dev202611041319+abc1234`, which sorts above beta.5 and below beta.6 (after a stable `v0.5.0`:
  `0.5.1-dev202611041319+abc1234`). Two dev builds on one day are two versions, so Home Assistant offers the newer
  one; a rebuild of the same commit is the same version. Home Assistant ignores everything after `+` and orders
  versions with AwesomeVersion, which reads `beta5-dev<time>` as one pre-release but cannot order
  `beta.5.dev.<time>` against a beta tag at all: keep the compound form. Its notes are the commit titles since the
  last dev build, linked to that commit range; no release-notes file. Only the newest dev build is served.

A panel on a channel is offered that channel's build whenever it differs from the one it runs, so switching from Dev
to Beta offers the beta even when it is older. Workbench builds (`panel wb build`, a `+wip` or build-stamped version)
are never published.

Pushing a tag starts `.github/workflows/release.yml`. It waits for CI and Firmware to be green on the tagged commit,
builds both boards, checks each image says the version (never `+wip`), and publishes:

- the tag's release with its images (a pre-release for a beta);
- the channel's asset (`firmware` or `firmware-beta`), and moves that channel tag to the build;
- a docs deploy, which serves `manifest.json` + `ota.json` (stable) and `manifest-beta.json` + `ota-beta.json` (beta)
  per board, beside the dev channel's `ota-dev.json`. The Install page offers Stable / Beta while a beta exists.

## Screen channels

The docs site and the README show every screen, each with its channel, from one source: `renderer/features.json`.

- **Stable**: in the latest stable release (`release.stable`). No badge.
- **Beta**: in a released beta, not yet in a stable release. A "Beta" badge on the docs, a "BETA" chip on the README
  picture.
- **Coming soon**: in no release yet: on `main` and in the docs (`firmware: false`, or `since: "next"`), or still
  being designed on the `wip` branch (`wip`: the docs' Coming soon page, `/screens/coming-soon/`, and the README's
  Coming soon strip, `tools/readme_assets.mjs --coming-soon`). A quieter "Coming soon" badge and chip. When a wip
  screen comes to `main` it moves from `wip` into `features`.

Screens are what people put on the panel or a Home deck. What every panel has (set up, Settings, the idle clock, the
Home Assistant link, API keys, update channels, report a problem) is listed in `panel_features` instead: the docs and
the README show these under **On every panel**, never among the screens, with the same channel marks. An entry is one
of the panel's own screens (its channel is that screen's) or a setting with its own `since`.

Every feature with `firmware: true` says `since`: the version its screens first shipped in, or `next`. A screen that
shipped later than the rest of its feature has its own entry in `screen_since`. `node renderer/tools/features.mjs check`
(build.sh, check.sh, CI) fails when one is missing, and, where the release tags are fetched (CI fetches them), when one
disagrees with the tags: a `next` screen that is on a tagged firmware, a beta screen whose `since` is not the first tag
it is in. `features.mjs channels` lists every screen's channel; `features.mjs since` prints what the tags say.

Before tagging V: `node renderer/tools/features.mjs since --write V` turns every `next` into V (and, for a stable V,
sets `release.stable`, which moves every beta screen to Stable), then render the README pictures again
(`renderer/tools/readme_assets.mjs`: the chip is drawn from the channel; `tools/channels.test.mjs` fails while a picture
says another channel).

## Release notes

Every version has `release-notes/<version>.md`, written for the people who install the panel, not as a commit list.
A tag without it fails before anything builds. Draft the next version's file as work lands; check it with
`node firmware/release-notes.mjs check <version>`.

- A headline paragraph: what this version means for someone with a panel on the wall.
- `## New`, `## Improved`, `## Fixed`: each bullet starts with a bold lead in plain language, then a sentence or two,
  then its issue (`#N`). One of the three has bullets.
- Docs never lag a release: every bullet of `## New` and `## Improved` links the docs page that explains it
  (`https://mrdarrengriffin.github.io/facet/<page>/`, a page that exists in `site/src/pages`), or says
  `(no docs: <why>)`. `check` (and so the tag's release workflow) fails otherwise: write the docs with the feature.
- `## After updating`: only when someone has to do something (a setting moved or went, a step on the panel).
- `## Known issues`: what is still wrong, with its issue.

The release workflow uses the file as the GitHub release body (plus the full-changelog link). The OTA manifest's
`summary` is its short version: the headline, After updating in full, then the bold leads (at most 1200 characters).
Home Assistant shows that summary as the release notes in the panel's update dialog, with `release_url` (the GitHub
release) as the link to the rest. The manifest also lists every version's own short summary since the last stable
release (`notes`, `node firmware/release-notes.mjs entries V`); the panel shows those newer than the version it runs,
newest first, so a panel that skipped versions sees what changed in each. A section with nothing under it is left out. To change a published release's notes: edit the file, commit, then
`node firmware/release-notes.mjs body V vPREV > notes.md && gh release edit vV --notes-file notes.md`; the update card
picks up the new summary only with the next build of that channel.

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
3. Read the root README: the screens table is every screen the docs list, with its channel chip, and On every
   panel marks each panel feature's channel; the panels table
   says exactly what is missing, if anything. `node renderer/tools/features.mjs since --write V` (Screen channels,
   below), then `(cd renderer && node tools/readme_assets.mjs)` so the README pictures carry the new chips.
4. `node firmware/release-notes.mjs check V`: the notes are written and read well (Release notes, above).
5. `node firmware/release-prep.mjs V`: sets `facet_version` in both `esphome/facet-*.yaml`, publishes the Updates
   entry dated today. `git diff --stat` shows those files only.
6. `(cd site && FACET_DOCS_BASE=/facet/ FACET_DOCS_OUT=dist.release npm run build)`: `check-dist: N pages, 0 faults`.
7. Commit "Release: V", `git tag -a vV -m "Facet V"`, `git push origin main vV`.
8. Watch the release workflow (`gh run list --workflow release.yml`); then
   `https://mrdarrengriffin.github.io/facet/firmware/<board>/manifest.json` says V.
9. Install from the docs site on a fresh panel (the checklist above); the 24 h run on this build starts.

To build the images locally instead (no publishing): `esphome/release.sh V [out dir]` builds from a worktree of the tag.

## Task references

Code comments and commit messages that say `facet-NNN` refer to an item on the public project board,
<https://github.com/users/mrdarrengriffin/projects/3>.
