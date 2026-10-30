// GitHub screen (specs/github.json: Page > Header (its title follows the view) > Body: the summary (Hero block, a
// Grid of Status rows, More row), all pull requests (two Section headers + Grids of Status rows) and one pull request
// (GitHub PR: the words + the QR block, the checks as a Grid of Status lines)), on the github feed panel.github
// (core/github_source.cpp, CONTRACT.md C51) and its Home card (C52). Home Assistant is faked (tools/functional.mjs
// FakeHA); the sensor is made up. Approved original: Astro packages/ui/src/screens/github.ts (was components/
// github_view.cpp + github_list.cpp + github_pr.cpp, three specs).
const S = 'sensor.test_github';
async function open(t, params) { await t.mount('spec', { screen: 'github', ...params }); return t; }
const deck = (params) => ({ v: 2, rev: 9, start: 'h', pages: [
  { id: 'h', kind: 'home', name: 'Test', cards: [{ t: 'page', page: 'g', size: 'l' }] },
  { id: 'g', kind: 'github', params }] });

export default {
  id: 'github_screen', family: 'screens', about: 'The GitHub screen on the github feed: summary, all pull requests, one pull request, Back, the HA adapter, Home card.',
  tests: [
    { name: 'sample: three need you, the top three, all pull requests', async run(t) {
      await open(t, { source: 'sample' });
      const tx = t.texts();
      t.has(tx, /^3$/, 'the hero'); t.has(tx, /^need you$/i, 'need you');
      t.has(tx, /^add a quiet hours schedule/i, 'a row'); t.has(tx, /^ci fail$/i, 'a failing row says so');
      t.has(tx, /^all pull requests · 3 more$/i, 'the more row');
    } },
    { name: 'a row opens the pull request; Back returns; More opens the list', async run(t) {
      await open(t, { source: 'sample' });
      t.tap(t.find(/^fix the sunrise time/i)); t.wait(400);
      let tx = t.texts();
      t.has(tx, /^#1290$/, 'the header names it'); t.has(tx, /^scan to open$/i, 'the QR block');
      t.has(tx, /^checks · 3 · 1 failing$/i, 'the checks line'); t.has(tx, /^test$/i, 'a check');
      t.tap({ clk: 0 }); t.wait(400);
      t.has(t.texts(), /^need you$/i, 'Back: the summary');
      t.tap(t.find(/^all pull requests/i)); t.wait(400);
      tx = t.texts();
      t.has(tx, /^pull requests$/i, 'the list header'); t.has(tx, /^your prs · 3$/i, 'your own');
      t.has(tx, /^docs: getting started/i, 'one of yours');
    } },
    { name: 'ha: an unavailable sensor says offline', async run(t) {
      t.ha.s[S] = { '': 'unavailable' };
      await open(t, { source: 'ha', sensor: S });
      t.has(t.texts(), /^github offline$/i, 'offline');
    } },
    { name: 'Home card (C52): how many need you on the same feed', async run(t) {
      await t.device(deck({ source: 'sample' }));
      const c = t.part('card');
      t.ok(c, 'the page card'); t.ok(c.texts.some((x) => /to review · 3 open/i.test(x)), 'the card line');
    } },
  ],
};
