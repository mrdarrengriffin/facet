// Facet editor v1's page, shared by the workbench (src/pages/wasm/editor-v1.astro) and the bundle a panel serves
// (scripts/editor-device.mjs): the boot data (built at build time from the renderer's catalogue, what a panel serves at
// /fw/schema) and the page's markup. The client is src/wasm/editor-v1.ts.
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

/** the boot JSON (escaped for a <script type="application/json">): repo = the facet repo root; panel = a panel's bundle
 *  (no test fixture, no factory deck file: the panel answers both) */
export function v1Boot(repo, { panel = false } = {}) {
  const R = join(repo, 'renderer');
  const rj = (p) => JSON.parse(readFileSync(p, 'utf8'));
  const schema = rj(join(R, 'catalogue/schema.json'));
  const role = (id) => { try { return rj(join(R, `specs/${id}.json`)).role ?? 'page'; } catch { return 'page'; } };
  const screens = Object.fromEntries(Object.entries(schema.screens).filter(([id, s]) => s.kind === 'spec' && role(id) !== 'detail'));  // pages only: a detail screen opens from a card
  const fixture = rj(join(repo, 'workbench/tests/editor/fixtures/sources.json'));
  const accents = rj(join(R, 'tokens/colour.json')).accent;
  const scope = readFileSync(join(repo, 'workbench/EDITOR-V1.md'), 'utf8');
  const own_max = Number(readFileSync(join(R, 'core/spec.h'), 'utf8').match(/\bhomes = (\d+)/)[1]);  // the deck check's budget for your own screens
  // the workbench's mock panel starts from a deck with every screen (tests/fixtures/deck-full.json; a panel's own
  // factory deck is one empty Home)
  return JSON.stringify({
    screens, settings: schema.settings, deck: panel ? { start: '', pages: [] } : rj(join(R, 'tests/fixtures/deck-full.json')), options: panel ? {} : fixture.options, accents, scope, hash: schema.hash, own_max,
    cards: schema.spec.cards, grid_sizes: schema.spec.grid_sizes ?? '1x1,2x1,2x2,half', card_max: schema.spec.budget.cards,
  }).replace(/</g, '\\u003c');
}

/** the page's body (inside <body>); foot = the footer's words for where it runs */
export function v1Body(boot, foot) {
  return `<div class="v1" id="v1">
  <header class="v1-top">
    <div class="v1-brand"><span class="v1-mark" aria-hidden="true"></span><b>Facet</b><span class="v1-meta" id="v1-where"></span></div>
    <b class="v1-title" id="v1-title"></b>
    <nav class="v1-tabs" id="v1-tabs" aria-label="Screens, in swipe order"></nav>
    <div class="v1-send">
      <div class="seg sm" id="v1-device" role="group" aria-label="Panel"></div>
      <button class="btn ghost" id="v1-undo" type="button">↶ Undo</button>
      <button class="btn ghost icon" id="v1-look" type="button"></button>
      <button class="btn" id="v1-settings" type="button" aria-pressed="false">Settings</button>
      <button class="btn primary" id="v1-save" type="button">Save</button>
    </div>
  </header>
  <main class="v1-main" id="v1-main"><p class="dim">Loading the renderer</p></main>
  <footer class="v1-foot">${foot}<button class="link" id="v1-scope" type="button">Scope</button></footer>
</div>
<dialog class="v1-dialog" id="v1-dialog"></dialog>
<script type="application/json" id="v1-boot">${boot}</script>`;
}
