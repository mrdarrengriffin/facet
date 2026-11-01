# Editor end-to-end tests

**State:** 48 of 71 pass. They moved here from the private host with the editor. The rest fail because their fixture
deck and expectations still assume features that are on the `wip` branch (the cover and climate Home cards, the pages
of held screens) and a house fixture that was replaced with anonymous entities. Bringing them back to green is part
of the editor work (plan Phase 5.5); they are in no check tier yet.

Playwright (library) + `node:test` against the real workbench editor page (`/wasm/editor`, the real WASM engine) and the
device bundle (`scripts/editor-device.mjs`, served from a stubbed panel). Each test asserts what a user would see or
what the panel would get: what the engine actually drew (the `window.__fe` test bridge in `src/wasm/editor.ts` reads
each preview stage's labels via `fw_audit`), the deck the editor holds, what Save sends.

Run (the workbench up on http://localhost:4322, `workbench/run.sh`; Chromium at /usr/bin/chromium):

    npm run test:editor                       # from workbench/; ~2 min, all files, one at a time
    node --test tests/editor/drag.test.mjs    # one file

`WORKBENCH_URL` / `CHROMIUM_PATH` override the defaults. The sources are fixed (`fixtures/sources.json`, anonymous entities, replaces
`/api/wasm/sources`); HA states are stubbed and HA actions are recorded, never sent.

| File | Covers |
|---|---|
| `add-card.test.mjs` | every card type, both panels (480 + 720): the modal, the Home preview, the thumbnail draw THAT card; one Undo |
| `card-edit.test.mjs` | click = that card alone; Esc / backdrop / Close deselect; header actions; label (live, one Undo); entity (wrong domain refused); size, tap, duplicate, move, remove; Undo / Redo |
| `drag.test.mjs` | pick-up drag of Home cards, screen tiles, Homes: new order in deck + preview, one Undo, Esc = back |
| `homes-screens.test.mjs` | add / rename / start page / remove Homes (page cards to it go too), last Home kept; add screen, switch off |
| `save.test.mjs` | Save sends exactly the shown deck (rev + 1); load round-trip draws the same; a refused deck is said so |
| `device-host.test.mjs` | panel-served editor, both panels: only the deck's entities asked from `/fw/ha`; add card + Save posts the deck; a refused Save keeps the edits |

Hooks: `data-testid` on the preview overlay (`preview-hits`), `add-card`, `card-modal`, `home-tile`, `screen-tile`,
`add-home`, `add-screen`, `home-settings`, `home-start`, `home-remove`; segmented buttons carry `data-v`.
