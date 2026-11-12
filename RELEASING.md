# Releasing Facet

## Versions

Every release follows this scheme (the docs explain it to users on the
[Versions](https://mrdarrengriffin.github.io/facet/help/versions/) page):

- Facet as a whole is a beta product while it is 0.x.
- **0.6, 0.7, 0.8…**: feature releases, one per milestone, each with a goal.
- **0.6.1, 0.6.2…**: fixes only, between feature releases, only when something can't wait.
- **0.6.0-beta.1, 0.6.0-beta.2…**: pre-releases of the next version, for the Beta channel only. These are the only
  `-beta.N` tags.
- **1.0**: when the editor (#82) and config backup (#170) are done, every test passes, and there have been a few weeks
  without a nasty bug; when you'd give it to a stranger with no caveats. After 1.0, the same pattern (1.1, 1.0.1);
  2.0 only if decks or settings ever break.
- 0.5.0 (the first plain release) closed the 0.5 beta line; panels on 0.5.0-beta.10 saw it as an update (semver:
  0.5.0 > 0.5.0-beta.10).

Planning: each version is a GitHub milestone named after it (`0.6.0`), its description saying the goal; the issues
picked for it go on it. Every new issue gets a milestone when it is picked, and the same version as its **Release**
value on the [project board](https://github.com/users/mrdarrengriffin/projects/3) (1.0, After 1.0 or Someday until it is
picked). A fix release gets its own milestone (`0.6.1`) only when one is needed.

## Channels

A panel's Update channel (Settings, and the Update channel select in Home Assistant) picks which manifest its Firmware
update entity checks. The default is Release; a fresh panel starts on the channel of the build it was flashed with. A
saved `stable` (the channel's old name) reads as Release.

- **Release**: every finished version, a tag `vX.Y.Z` (0.5.0, 0.6.0, 0.6.1). Its `esphome/facet-*.yaml` files must
  already say that version. `ota.json`, and the Install page's default (`manifest.json`). A release goes to Beta as
  well (`ota-beta.json`, `manifest-beta.json`), so Beta never lags behind Release.
- **Beta**: where people test the next version's screens and features before it ships, without being on nightly
  builds. Its builds are pre-releases of the next version, a tag `vX.Y.Z-beta.N` (0.6.0-beta.1), published only to
  `ota-beta.json` and the Install page's Beta switch (`manifest-beta.json`), as GitHub pre-releases with short notes.
  When testing is done, the finished 0.6.0 goes to Release, and Beta gets it too.
- **Dev (nightly)**: a build of `main` every night by `.github/workflows/dev.yml`, untested beyond CI. `ota-dev.json`
  only: the Install page never offers it. It builds once a night when `main` moved since the last dev build (the
  `firmware-dev` tag) and CI and Firmware are green on it; `gh workflow run dev.yml` builds one now. Its version is the
  last tag's plus the commit's time (UTC, to the minute) and the commit: after `v0.6.0-beta.1`,
  `0.6.0-beta1-dev202611041319+abc1234`, which sorts above beta.1 and below beta.2 (after a release `v0.6.0`:
  `0.6.1-dev202611041319+abc1234`). Two dev builds on one day are two versions, so Home Assistant offers the newer
  one; a rebuild of the same commit is the same version. Home Assistant ignores everything after `+` and orders
  versions with AwesomeVersion, which reads `beta1-dev<time>` as one pre-release but cannot order
  `beta.1.dev.<time>` against a beta tag at all: keep the compound form. Its notes are the commit titles since the
  last dev build, linked to that commit range; no release-notes file. Only the newest dev build is served.

A panel is offered a channel's build only when it is newer than the one it runs (`esphome/board/fw_notes.h` newer()):
moving from Dev back to Beta or Release waits for that channel's next version. Workbench builds (`panel wb build`, a
`+wip` or build-stamped version) are never published.

Pushing a tag starts `.github/workflows/release.yml`. It refuses anything but `vX.Y.Z` and `vX.Y.Z-beta.N`, waits
for CI and Firmware to be green on the tagged commit, builds both boards, checks each image says the version (never
`+wip`), and publishes:

- the tag's GitHub release with its images: the latest release for `vX.Y.Z`, a pre-release for a beta;
- the channel's asset, moving that channel tag to the build: a release writes `manifest.json` + `ota.json` and the same
  as `manifest-beta.json` + `ota-beta.json`, and is both the `firmware` and the `firmware-beta` asset; a beta writes
  the beta pair only, as `firmware-beta`;
- a docs deploy, which serves `manifest.json` + `ota.json` (Release) and `manifest-beta.json` + `ota-beta.json` (Beta)
  per board, beside the Dev channel's `ota-dev.json`. The Install page offers Release / Beta.

## Screen channels

The docs site and the README show every screen, each with its channel, from one source: `renderer/features.json`.

- **Released** (`stable` in the data): in the latest release (`release.stable`). No badge.
- **Beta**: in a released beta of the next version, not yet in a release. A "Beta" badge on the docs, a "BETA" chip
  on the README picture.
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

Before tagging V: `node renderer/tools/features.mjs since --write V` turns every `next` into V (and, for a release V,
sets `release.stable`, which moves every beta screen to Released), then render the README pictures again
(`renderer/tools/readme_assets.mjs`: the chip is drawn from the channel; `tools/channels.test.mjs` fails while a picture
says another channel).

## Release notes

Every version has `release-notes/<version>.md`, written for the people who install the panel, not as a commit list.
A beta's notes are short: the headline and what to try, with what changed since the last beta; the release's notes
then say everything the version brings, beta or not.
A tag without it fails before anything builds. Draft the next version's file as work lands; check it with
`node firmware/release-notes.mjs check <version>`.

The notes read like a popular product's release notes: short, scannable, a bit of fun, never a wall of text. A heading
may start with an emoji, the same set every time (`check` ignores it):

- **Intro**: 2-3 sentences, the big themes, before the first section.
- `## ⭐ Highlights` (a feature release): the 3-4 headline features, each a `### [Feature name](docs page)` heading,
  one line of what it does for you with its issues small at the end (`<sub>#12 #14</sub>`), 2-3 short bullets for the
  key points, then a picture.
- `## ✨ New`, `## 🛠️ Improved`, `## 🐛 Fixed`: one bullet per change, `- **[Feature name](docs page)**: one line.
  <sub>#N</sub>`, then `  - ` sub-points only where they help. No paragraphs. The feature name is the link (never a
  bare URL at the end). One of Highlights, New, Improved or Fixed has something in it.
- Docs never lag a release: every Highlight and every bullet of New and Improved links the docs page that explains it
  (`https://mrdarrengriffin.github.io/facet/<page>/`, a page that exists in `site/src/pages`), or carries
  `<!-- no docs: <why> -->` (hidden on GitHub). `check` (and so the tag's release workflow) fails otherwise.
- `## 🧪 Since <last beta>` (a release only, optional, after Fixed): what is new to the Beta channel's testers since the
  last beta (`## 🧪 Since 0.6.0-beta.3`). Short bullets with bold leads; body only, never in the summary.
- `## ⬆️ After updating`: only when someone has to do something (a setting moved or went, a step on the panel).
- `## ⚠️ Known issues`: what is still wrong, one line each, with its issue.
- **Pictures**: one per Highlight, plus before / after pairs where a screen changed, all made by
  `cd renderer && node tools/release_shots.mjs V <name>=<spec> ...` (its header lists the forms) into
  `release-notes/img/V/`:
  - **Panel pictures** are rendered by the renderer with the docs site's made-up house (never a real home's data),
    both panels side by side where it helps, every screen at the same size (360), an older README picture from git as
    the "before". Each is framed as the Waveshare's front: a 2 px white line, then 24 px of off-black faceplate (`#16181b`)
    round the screen, all square (no radius, no shadow). Each comes in the dark and the light theme
    (`<name>-dark.png`, `<name>-light.png`), switched with the reader's mode as the README does:
    `<picture><source media="(prefers-color-scheme: dark)" srcset=".../<name>-dark.png"><img src=".../<name>-light.png" width="640" alt="..."></picture>`.
  - **Editor pictures** are fresh screenshots of a real panel's editor showing made-up data (laptop ~1440 × 900 at
    2x, phone), put in plain: no frame, no radius. Never the old two-panel `site/public/editor/editor.png`.
  - Commit them before the tag and link them by raw URL,
    `https://raw.githubusercontent.com/mrdarrengriffin/facet/vV/release-notes/img/V/<file>` (notes edited after the
    tag: the commit that added the pictures in place of `vV`). Every `<img>` has alt text saying what it shows; keep
    the set small (a few hundred KB). `check` fails on any other picture.
- **Cross-check, once the notes are written**: every linked docs page and `#anchor` exists and says what the bullet
  says, and the release-day step 3a list (README and its pictures, CONTRIBUTING, AGENTS.md and any agent docs, the
  docs' /ai pages and llms.txt / llms-full.txt, the panel's /llms.txt + /agents.md) matches the notes. Fix what is
  stale before the tag.
- **All changes, at the end of every version's notes (beta included; `check` fails without it):** everything that went
  in, under the written sections, collapsed (closed by default) so the notes stay short: the issues closed as
  completed in the version's milestone(s) (a release: `X.Y.Z` and every `X.Y.Z-*`, so all its betas; a beta or a fix
  release: its own), grouped by type label into Breaking changes (`breaking`), Features (`feature`), Fixes (`bug`)
  and Other (anything else: tasks, tests), each `- <issue title> #N` by number, then the compare link. No commits,
  PRs or area tags, so every issue in the milestone needs its type label before the list is made.
  Generated, never hand-made, as the last thing before the tag (it needs `gh`):
  `node firmware/release-notes.mjs changes V vPREV >> release-notes/V.md` (vPREV = the previous tag: the compare
  link runs from it). The block is:

  ```
  <details><summary>All changes (N)</summary>

  ### Features (n)

  - Title #12
  ...

  **Full commit history:** https://github.com/mrdarrengriffin/facet/compare/vPREV...vV

  </details>
  ```

  N = its `- ` lines (`check` counts them). The blank line after `<summary>` lets GitHub render the list inside. It is
  never in the HA summary or the manifest's notes; the docs site does not show release notes (Updates entries are
  written separately), so nothing else renders it.

The release workflow uses the file as the GitHub release body (the All changes block carries the compare link). The OTA manifest's
`summary` is its short version, plain text (no links, pictures, issue numbers or emoji): the intro, After updating in
full, then the Highlights' and each section's feature names (at most 1200 characters).
Home Assistant shows that summary as the release notes in the panel's update dialog, with `release_url` (the GitHub
release) as the link to the rest. The manifest also lists every version's own short summary since the last
release (`notes`, `node firmware/release-notes.mjs entries V`); the panel shows those newer than the version it runs,
newest first, so a panel that skipped versions sees what changed in each. A section with nothing under it is left out. To change a published release's notes: edit the file, commit, then
`node firmware/release-notes.mjs body V vPREV > notes.md && gh release edit vV --notes-file notes.md`; the update card
picks up the new summary only with the next build of that channel.

## Quality checklist (a person, on glass, before a release tag)

Run on a fresh Waveshare and a fresh Guition. Every line is a yes or the release waits. The workbench shows the same
list (`workbench/src/lib/checklist.ts`).

- Install from the docs site in Chrome on a panel that has never run Facet. First boot reaches set up without help.
- Set up: join Wi-Fi, link Home Assistant, land on Home with the factory deck.
- Open the panel's address in a browser: the editor loads, reorder the deck, change one screen's entity, see it on
  the glass within a few seconds. Undo puts the last save back; Back to factory starts over.
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

V = the version: a release (`0.6.0`, a fix release `0.6.1`) or a beta of the next version (`0.6.0-beta.1`). Every
tag follows the Versions scheme above; the milestone named V (for a beta, its version's: `0.6.0`) is what it ships.
A failed check stops the day.

1. `git switch main && git pull && git status --short`: empty, and the last nightly gate green.
2. `(cd renderer && node tools/gen_catalogue.mjs) && git status --short`: empty (else commit "Catalogue: V").
3. Read the root README: the screens table is every screen the docs list, with its channel chip, and On every
   panel marks each panel feature's channel; the panels table
   says exactly what is missing, if anything. `node renderer/tools/features.mjs since --write V` (Screen channels,
   below), then `(cd renderer && node tools/readme_assets.mjs)` so the README pictures carry the new chips.
3a. **Everything up to date for V** (every tag, beta included). Go through the milestone's closed issues and
   `git log <last tag>..main`, and update whatever they changed:
   - root `README.md` (screens/panels tables, feature list, pictures), `CONTRIBUTING.md`, `AGENTS.md`, `PRODUCT.md`,
     `renderer/CONTRACT.md`, `RELEASING.md` itself;
   - the docs site (`site/src`): every page a change touches (screens, components, editor, settings, install,
     /ai and the agent docs, roadmap/progress), with new screenshots at 480 and 720 where the UI changed;
   - the docs' `llms.txt` / `llms-full.txt` (`site/scripts/llms.mjs`, checked by `check-dist`);
   - the panel's own agent docs (`/llms.txt` + `/agents.md`, `renderer/tools/gen_agents.mjs` → `firmware/bundle.mjs`):
     regenerate them so the shipped firmware describes this release's components, props and API;
   - the catalogue + schema (step 2) and any route the release added or changed (`/fw/*`).
   Nothing user-visible ships undocumented. List what was updated in the release PR/commit.
4. `node firmware/release-notes.mjs changes V vPREV >> release-notes/V.md`, then `node firmware/release-notes.mjs check V`:
   the notes are written, read well, have their pictures and end with All changes; then the cross-check (Release
   notes, above) against 3a's list, fixing what is stale.
5. `node firmware/release-prep.mjs V`: sets `facet_version` in both `esphome/facet-*.yaml`, publishes the Updates
   entry titled "Facet V" (a trailing `.0` dropped: "Facet 0.6"; draft it first) dated today. `git diff --stat` shows
   those files only. A beta changes nothing here (its YAMLs are stamped at build, it gets no Updates entry).
6. `(cd site && FACET_DOCS_BASE=/facet/ FACET_DOCS_OUT=dist.release npm run build)`: `check-dist: N pages, 0 faults`.
7. Commit "Release: V", `git tag -a vV -m "Facet V"`, `git push origin main vV`.
8. Watch the release workflow (`gh run list --workflow release.yml`); then
   `https://mrdarrengriffin.github.io/facet/firmware/<board>/ota-beta.json` says V, and for a release `manifest.json`
   and `ota.json` too. After a release, close its milestone.
9. Install from the docs site on a fresh panel (the checklist above); the 24 h run on this build starts.

To build the images locally instead (no publishing): `esphome/release.sh V [out dir]` builds from a worktree of the tag.

## Task references

Code comments and commit messages that say `facet-NNN` refer to an item on the public project board,
<https://github.com/users/mrdarrengriffin/projects/3>.
